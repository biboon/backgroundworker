#include "backgroundworker.h"

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>


#ifndef BW_SIGRT_WORKEREXITS
#define BW_SIGRT_WORKEREXITS (SIGRTMIN + 1)
#endif


struct backgroundworker {
	pid_t pid;
	pthread_t tid;
	pthread_mutex_t lock;

	void (*DoWork            )(bw_t *worker, void *arg);
	void (*ProgressChanged   )(bw_t *worker, int percentage);
	void (*RunWorkerCompleted)(bw_t *worker, void *res);

	int CancellationPending;
	int IsBusy;
	int WorkerReportsProgress;

	struct {
		void *p;
		size_t len;
	} Argument, Result;
};


/* BackgroundWorker functions */

static void *StartWorkerRoutine(void *arg);
static void  WorkerExitsRoutine(int signo, siginfo_t *info, void *context);

bw_t *bw_Create(void)
{
	static int initialized = 0;
	bw_t *worker;

	if (!initialized)
	{
		struct sigaction act;
		act.sa_sigaction = WorkerExitsRoutine;
		act.sa_flags = SA_SIGINFO;
		sigfillset(&act.sa_mask);
		sigaction(BW_SIGRT_WORKEREXITS, &act, NULL);
		initialized = 1;
	}

	worker = calloc(1, sizeof(*worker));
	if (worker == NULL)
		return NULL;

	if (pthread_mutex_init(&worker->lock, NULL) != 0)
	{
		free(worker);
		return NULL;
	}

	worker->pid = getpid();

	return worker;
}

void bw_Destroy(bw_t *worker)
{
	pthread_mutex_destroy(&worker->lock);
	free(worker->Argument.p);
	free(worker->Result.p);
	free(worker);
}


void bw_DoWork(bw_t *worker, void (*DoWork)(bw_t *, void *))
{
	pthread_mutex_lock(&worker->lock);
	worker->DoWork = DoWork;
	pthread_mutex_unlock(&worker->lock);
}

void bw_ProgressChanged(bw_t *worker, void (*ProgressChanged)(bw_t *, int))
{
	pthread_mutex_lock(&worker->lock);
	worker->ProgressChanged = ProgressChanged;
	pthread_mutex_unlock(&worker->lock);
}

void bw_RunWorkerCompleted(bw_t *worker, void (*RunWorkerCompleted)(bw_t *, void *))
{
	pthread_mutex_lock(&worker->lock);
	worker->RunWorkerCompleted = RunWorkerCompleted;
	pthread_mutex_unlock(&worker->lock);
}

void bw_CancelAsync(bw_t *worker)
{
	pthread_mutex_lock(&worker->lock);
	worker->CancellationPending = 1;
	pthread_mutex_unlock(&worker->lock);
}

int bw_CancellationPending(bw_t *worker)
{
	int ret;

	pthread_mutex_lock(&worker->lock);
	ret = worker->CancellationPending;
	pthread_mutex_unlock(&worker->lock);

	return ret;
}

int bw_IsBusy(bw_t *worker)
{
	int ret;

	pthread_mutex_lock(&worker->lock);
	ret = worker->IsBusy;
	pthread_mutex_unlock(&worker->lock);

	return ret;
}

void bw_WorkerReportsProgress(bw_t *worker, int b)
{
	pthread_mutex_lock(&worker->lock);
	worker->WorkerReportsProgress = !!b;
	pthread_mutex_unlock(&worker->lock);
}


int bw_RunWorkerAsync(bw_t *worker, void *arg, size_t len)
{
	pthread_attr_t attr;

	pthread_mutex_lock(&worker->lock);

	if (worker->IsBusy)
	{
		pthread_mutex_unlock(&worker->lock);
		return -1;
	}

	if (arg != NULL && len != 0)
	{
		if (worker->Argument.len < len)
		{
			void *ptr = realloc(worker->Argument.p, len);
			if (ptr == NULL)
			{
				pthread_mutex_unlock(&worker->lock);
				return -1;
			}

			worker->Argument.p   = ptr;
			worker->Argument.len = len;
		}

		memcpy(worker->Argument.p, arg, len);
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&worker->tid, &attr, StartWorkerRoutine, worker);

	pthread_mutex_unlock(&worker->lock);

	return 0;
}

void bw_WorkerExit(bw_t *worker, void *res, size_t len)
{
	if (res == NULL || len == 0) return;

	pthread_mutex_lock(&worker->lock);

	if (worker->Result.len < len)
	{
		void *ptr = realloc(worker->Result.p, len);
		if (ptr == NULL)
		{
			pthread_mutex_unlock(&worker->lock);
			return;
		}

		worker->Result.p   = ptr;
		worker->Result.len = len;
	}

	memcpy(worker->Result.p, res, len);

	pthread_mutex_unlock(&worker->lock);
}


void bw_ReportProgress(bw_t *worker, int value)
{
	void (*ProgressChanged)(bw_t *, int);

	if (value > 100) value = 100;
	if (value <   0) value =   0;

	pthread_mutex_lock(&worker->lock);
	ProgressChanged = worker->WorkerReportsProgress ? worker->ProgressChanged : NULL;
	pthread_mutex_unlock(&worker->lock);

	if (ProgressChanged != NULL)
		ProgressChanged(worker, value);
}


void *StartWorkerRoutine(void *_worker)
{
	bw_t *worker = (bw_t *) _worker;

	void (*DoWork)(bw_t *, void *);
	void *arg;
	pid_t pid;

	pthread_mutex_lock(&worker->lock);
	worker->CancellationPending = 0;
	worker->IsBusy = 1;
	DoWork = worker->DoWork;
	arg = worker->Argument.p;
	pid = worker->pid;
	pthread_mutex_unlock(&worker->lock);

	if (DoWork != NULL)
		DoWork(worker, arg);

	sigqueue(pid, BW_SIGRT_WORKEREXITS, (union sigval) _worker);

	pthread_exit(NULL);
}

void WorkerExitsRoutine(int signo, siginfo_t *info, void *context)
{
	bw_t *worker = (bw_t *) info->si_value.sival_ptr;

	void (*RunWorkerCompleted)(bw_t *, void *);
	void *res;

	pthread_mutex_lock(&worker->lock);
	worker->IsBusy = 0;
	RunWorkerCompleted = worker->RunWorkerCompleted;
	res = worker->Result.p;
	pthread_mutex_unlock(&worker->lock);

	if (RunWorkerCompleted != NULL)
		RunWorkerCompleted(worker, res);
}
