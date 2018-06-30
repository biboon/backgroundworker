#define _POSIX_C_SOURCE 199506L
#include "worker.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>


#ifndef WORKER_SIGEXIT
#define WORKER_SIGEXIT (SIGRTMIN + 1)
#endif

#define WORKER_ISBUSY    0x01
#define WORKER_CANCELLED 0x02


struct worker {
	pid_t pid;
	pthread_t tid;
	pthread_attr_t attr;
	pthread_mutex_t lock;
	int flags;

	void (*doWork            )(worker_t *worker, void *arg);
	void (*runWorkerCompleted)(worker_t *worker, void *res);
	void (*progressChanged   )(worker_t *worker, int percentage);

	struct {
		void *ptr;
		size_t len;
	} argument, result;
};


static void *StartWorkerRoutine(void *arg);
static void  WorkerExitsRoutine(int signo, siginfo_t *info, void *context);


worker_t *worker_create(void)
{
	static int initialized = 0;
	worker_t worker = { 0 }, *pWorker;

	if (!initialized)
	{
		struct sigaction act;
		act.sa_sigaction = WorkerExitsRoutine;
		act.sa_flags = SA_SIGINFO;

		if (sigfillset(&act.sa_mask) == -1) return NULL;
		if (sigaction(WORKER_SIGEXIT, &act, NULL) == -1) return NULL;

		initialized = 1;
	}

	pWorker = (worker_t *)malloc(sizeof(worker_t));

	if (pWorker == NULL) goto out;
	if (pthread_mutex_init(&worker.lock, NULL) != 0) goto out_alloc;
	if (pthread_attr_init(&worker.attr) != 0) goto out_mutex;

	worker.pid = getpid();
	*pWorker = worker;

	return pWorker;

out_mutex:
	pthread_mutex_destroy(&worker.lock);
out_alloc:
	free(pWorker);
out:
	return NULL;
}

void worker_destroy(worker_t *worker)
{
	if (worker)
	{
		pthread_mutex_destroy(&worker->lock);
		pthread_attr_destroy(&worker->attr);
		free(worker->argument.ptr);
		free(worker->result.ptr);
		free(worker);
	}
}


void worker_doWork(worker_t *worker, void (*doWork)(worker_t *, void *))
{
	assert(worker);
	pthread_mutex_lock(&worker->lock);
	worker->doWork = doWork;
	pthread_mutex_unlock(&worker->lock);
}

void worker_progressChanged(worker_t *worker, void (*progressChanged)(worker_t *, int))
{
	assert(worker);
	pthread_mutex_lock(&worker->lock);
	worker->progressChanged = progressChanged;
	pthread_mutex_unlock(&worker->lock);
}

void worker_runWorkerCompleted(worker_t *worker, void (*runWorkerCompleted)(worker_t *, void *))
{
	assert(worker);
	pthread_mutex_lock(&worker->lock);
	worker->runWorkerCompleted = runWorkerCompleted;
	pthread_mutex_unlock(&worker->lock);
}


void worker_cancelAsync(worker_t *worker)
{
	assert(worker);
	pthread_mutex_lock(&worker->lock);
	worker->flags |= WORKER_CANCELLED;
	pthread_mutex_unlock(&worker->lock);
}

void worker_reportProgress(worker_t *worker, int value)
{
	void (*progressChanged)(worker_t *, int);

	assert(worker);
	pthread_mutex_lock(&worker->lock);
	progressChanged = worker->progressChanged;
	pthread_mutex_unlock(&worker->lock);

	if (progressChanged) progressChanged(worker, value);
}

int worker_cancellationPending(worker_t *worker)
{
	int ret;

	assert(worker);
	pthread_mutex_lock(&worker->lock);
	ret = worker->flags & WORKER_CANCELLED;
	pthread_mutex_unlock(&worker->lock);

	return ret;
}

int worker_isBusy(worker_t *worker)
{
	int ret;

	assert(worker);
	pthread_mutex_lock(&worker->lock);
	ret = worker->flags & WORKER_ISBUSY;
	pthread_mutex_unlock(&worker->lock);

	return ret;
}


int worker_run(worker_t *worker, void *arg, size_t len, int detachstate)
{
	assert(worker);
	assert(detachstate == WORKER_DETACHED || detachstate == WORKER_JOINABLE);

	pthread_mutex_lock(&worker->lock);

	if (worker->flags & WORKER_ISBUSY)
	{
		pthread_mutex_unlock(&worker->lock);
		return -1;
	}

	if (arg != NULL && len != 0)
	{
		if (worker->argument.len < len)
		{
			void *ptr = realloc(worker->argument.ptr, len);
			if (ptr == NULL)
			{
				pthread_mutex_unlock(&worker->lock);
				return -1;
			}

			worker->argument.ptr = ptr;
			worker->argument.len = len;
		}

		memcpy(worker->argument.ptr, arg, len);
	}

	worker->flags |= WORKER_ISBUSY;
	worker->flags &= ~WORKER_CANCELLED;

	switch (detachstate)
	{
		case WORKER_DETACHED: detachstate = PTHREAD_CREATE_DETACHED; break;
		case WORKER_JOINABLE: detachstate = PTHREAD_CREATE_JOINABLE; break;
	}

	pthread_attr_setdetachstate(&worker->attr, detachstate);
	pthread_create(&worker->tid, &worker->attr, StartWorkerRoutine, worker);

	pthread_mutex_unlock(&worker->lock);

	return 0;
}

void worker_exit(worker_t *worker, void *res, size_t len)
{
	int detachstate;
	void *retval = NULL;

	assert(worker);

	pthread_mutex_lock(&worker->lock);

	if (res != NULL && len != 0)
	{
		if (worker->result.len < len)
		{
			void *ptr = realloc(worker->result.ptr, len);
			if (ptr == NULL)
			{
				pthread_mutex_unlock(&worker->lock);
				pthread_exit(NULL);
			}

			worker->result.ptr = ptr;
			worker->result.len = len;
		}

		memcpy(worker->result.ptr, res, len);

		retval = worker->result.ptr;
	}

	worker->flags &= ~WORKER_ISBUSY;

	pthread_attr_getdetachstate(&worker->attr, &detachstate);
	if (detachstate == PTHREAD_CREATE_DETACHED)
	{
		union sigval value;
		value.sival_ptr = worker;
		sigqueue(worker->pid, WORKER_SIGEXIT, value);
	}

	pthread_mutex_unlock(&worker->lock);

	pthread_exit(retval);
}

int worker_join(worker_t *worker, void **retval)
{
	pthread_t tid;

	assert(worker);
	pthread_mutex_lock(&worker->lock);
	tid = worker->tid;
	pthread_mutex_unlock(&worker->lock);

	return pthread_join(tid, retval);
}

int worker_kill(worker_t *worker, int signo)
{
	pthread_t tid;

	assert(worker);
	pthread_mutex_lock(&worker->lock);
	tid = worker->tid;
	pthread_mutex_unlock(&worker->lock);

	return pthread_kill(tid, signo);
}


void *StartWorkerRoutine(void *_worker)
{
	worker_t *worker = (worker_t *)_worker;
	void (*doWork)(worker_t *, void *);
	void *arg;

	pthread_mutex_lock(&worker->lock);
	doWork = worker->doWork;
	arg = worker->argument.ptr;
	pthread_mutex_unlock(&worker->lock);

	if (doWork) doWork(worker, arg);

	worker_exit(worker, NULL, 0);
}

void WorkerExitsRoutine(int signo, siginfo_t *info, void *context)
{
	worker_t *worker = (worker_t *)info->si_value.sival_ptr;
	void (*runWorkerCompleted)(worker_t *, void *);
	void *res;

	pthread_mutex_lock(&worker->lock);
	runWorkerCompleted = worker->runWorkerCompleted;
	res = worker->result.ptr;
	pthread_mutex_unlock(&worker->lock);

	if (runWorkerCompleted) runWorkerCompleted(worker, res);
}
