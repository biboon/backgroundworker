#include "worker.h"

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>


#ifndef WORKER_SIGEXIT
#define WORKER_SIGEXIT (SIGRTMIN + 1)
#endif


struct worker {
	pid_t pid;
	pthread_t tid;
	pthread_attr_t attr;
	pthread_mutex_t lock;

	void (*doWork            )(worker_t *worker, void *arg);
	void (*progressChanged   )(worker_t *worker, int percentage);
	void (*runWorkerCompleted)(worker_t *worker, void *res);

	int cancellationPending;
	int isBusy;
	int workerReportsProgress;

	struct {
		void *p;
		size_t len;
	} argument, result;
};


static void *StartWorkerRoutine(void *arg);
static void  WorkerExitsRoutine(int signo, siginfo_t *info, void *context);


worker_t *worker_create(void)
{
	static int initialized = 0;
	worker_t *worker;
	int status = 0;

	if (!initialized)
	{
		struct sigaction act;
		act.sa_sigaction = WorkerExitsRoutine;
		act.sa_flags = SA_SIGINFO;
		sigfillset(&act.sa_mask);
		sigaction(WORKER_SIGEXIT, &act, NULL);
		initialized = 1;
	}

	worker = calloc(1, sizeof(*worker));
	if (worker == NULL)
		return NULL;

	status += pthread_mutex_init(&worker->lock, NULL);
	status += pthread_attr_init(&worker->attr);

	if (status != 0)
	{
		free(worker);
		return NULL;
	}

	worker->pid = getpid();

	return worker;
}

void worker_destroy(worker_t *worker)
{
	pthread_mutex_destroy(&worker->lock);
	pthread_attr_destroy(&worker->attr);
	free(worker->argument.p);
	free(worker->result.p);
	free(worker);
}


void worker_doWork(worker_t *worker, void (*doWork)(worker_t *, void *))
{
	pthread_mutex_lock(&worker->lock);
	worker->doWork = doWork;
	pthread_mutex_unlock(&worker->lock);
}

void worker_progressChanged(worker_t *worker, void (*progressChanged)(worker_t *, int))
{
	pthread_mutex_lock(&worker->lock);
	worker->progressChanged = progressChanged;
	pthread_mutex_unlock(&worker->lock);
}

void worker_runWorkerCompleted(worker_t *worker, void (*runWorkerCompleted)(worker_t *, void *))
{
	pthread_mutex_lock(&worker->lock);
	worker->runWorkerCompleted = runWorkerCompleted;
	pthread_mutex_unlock(&worker->lock);
}

void worker_cancelAsync(worker_t *worker)
{
	pthread_mutex_lock(&worker->lock);
	worker->cancellationPending = 1;
	pthread_mutex_unlock(&worker->lock);
}

int worker_cancellationPending(worker_t *worker)
{
	int ret;

	pthread_mutex_lock(&worker->lock);
	ret = worker->cancellationPending;
	pthread_mutex_unlock(&worker->lock);

	return ret;
}

int worker_isBusy(worker_t *worker)
{
	int ret;

	pthread_mutex_lock(&worker->lock);
	ret = worker->isBusy;
	pthread_mutex_unlock(&worker->lock);

	return ret;
}

void worker_reportsProgress(worker_t *worker, int b)
{
	pthread_mutex_lock(&worker->lock);
	worker->workerReportsProgress = !!b;
	pthread_mutex_unlock(&worker->lock);
}

int worker_join(worker_t *worker, void **retval)
{
	pthread_t tid;

	pthread_mutex_lock(&worker->lock);
	tid = worker->tid;
	pthread_mutex_unlock(&worker->lock);

	return pthread_join(tid, retval);
}

int worker_kill(worker_t *worker, int signo)
{
	pthread_t tid;

	pthread_mutex_lock(&worker->lock);
	tid = worker->tid;
	pthread_mutex_unlock(&worker->lock);

	return pthread_kill(tid, signo);
}


int worker_run(worker_t *worker, void *arg, size_t len, int detachstate)
{
	switch (detachstate)
	{
		case WORKER_DETACHED: detachstate = PTHREAD_CREATE_DETACHED; break;
		case WORKER_JOINABLE: detachstate = PTHREAD_CREATE_JOINABLE; break;
		default: return -1;
	}

	pthread_mutex_lock(&worker->lock);

	if (worker->isBusy)
	{
		pthread_mutex_unlock(&worker->lock);
		return -1;
	}

	if (arg != NULL && len != 0)
	{
		if (worker->argument.len < len)
		{
			void *ptr = realloc(worker->argument.p, len);
			if (ptr == NULL)
			{
				pthread_mutex_unlock(&worker->lock);
				return -1;
			}

			worker->argument.p   = ptr;
			worker->argument.len = len;
		}

		memcpy(worker->argument.p, arg, len);
	}

	worker->isBusy = 1;
	worker->cancellationPending = 0;

	pthread_attr_setdetachstate(&worker->attr, detachstate);
	pthread_create(&worker->tid, &worker->attr, StartWorkerRoutine, worker);

	pthread_mutex_unlock(&worker->lock);

	return 0;
}

void worker_exit(worker_t *worker, void *res, size_t len)
{
	int detachstate;
	void *retval = NULL;

	pthread_mutex_lock(&worker->lock);

	if (res != NULL && len != 0)
	{
		if (worker->result.len < len)
		{
			void *ptr = realloc(worker->result.p, len);
			if (ptr == NULL)
			{
				pthread_mutex_unlock(&worker->lock);
				pthread_exit(NULL);
			}

			worker->result.p   = ptr;
			worker->result.len = len;
		}

		memcpy(worker->result.p, res, len);

		retval = worker->result.p;
	}

	worker->isBusy = 0;

	pthread_attr_getdetachstate(&worker->attr, &detachstate);
	if (detachstate == PTHREAD_CREATE_DETACHED)
	{
		sigqueue(worker->pid, WORKER_SIGEXIT, (union sigval)(void *) worker);
	}

	pthread_mutex_unlock(&worker->lock);

	pthread_exit(retval);
}


void worker_reportProgress(worker_t *worker, int value)
{
	void (*progressChanged)(worker_t *, int);

	if (value > 100) value = 100;
	if (value <   0) value =   0;

	pthread_mutex_lock(&worker->lock);
	progressChanged = worker->workerReportsProgress ? worker->progressChanged : NULL;
	pthread_mutex_unlock(&worker->lock);

	if (progressChanged != NULL)
		progressChanged(worker, value);
}


void *StartWorkerRoutine(void *_worker)
{
	worker_t *worker = (worker_t *) _worker;

	void (*doWork)(worker_t *, void *);
	void *arg;

	pthread_mutex_lock(&worker->lock);
	doWork = worker->doWork;
	arg = worker->argument.p;
	pthread_mutex_unlock(&worker->lock);

	if (doWork != NULL)
		doWork(worker, arg);

	worker_exit(worker, NULL, 0);
}

void WorkerExitsRoutine(int signo, siginfo_t *info, void *context)
{
	worker_t *worker = (worker_t *) info->si_value.sival_ptr;

	void (*runWorkerCompleted)(worker_t *, void *);
	void *res;

	pthread_mutex_lock(&worker->lock);
	runWorkerCompleted = worker->runWorkerCompleted;
	res = worker->result.p;
	pthread_mutex_unlock(&worker->lock);

	if (runWorkerCompleted != NULL)
		runWorkerCompleted(worker, res);
}
