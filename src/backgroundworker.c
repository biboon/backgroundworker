#include "backgroundworker.h"

#include <stdlib.h>
#include <string.h>


struct backgroundworker_DoWorkEventArgs {
	void  *Argument,    *Result;
	size_t Argument_len, Result_len;
	int Cancel;
};

struct backgroundworker {
	pthread_t tid;
	pthread_mutex_t lock;

	void (*DoWork            )(bw_t *worker, bw_DoWorkEventArgs_t *);
	void (*ProgressChanged   )(bw_t *worker, int percentage);
	void (*RunWorkerCompleted)(bw_t *worker, void *);

	int CancellationPending;
	int IsBusy;
	int WorkerReportsProgress;

	bw_DoWorkEventArgs_t DoWorkEventArgs;
};


/* BackgroundWorker functions */

static void *bw_RunWorker(void *arg);

bw_t *bw_Create(void)
{
	bw_t *worker;

	worker = calloc(1, sizeof(*worker));
	if (worker == NULL)
	{
		return NULL;
	}

	if (pthread_mutex_init(&worker->lock, NULL) != 0)
	{
		free(worker);
		return NULL;
	}

	return worker;
}

void bw_Destroy(bw_t *worker)
{
	pthread_mutex_destroy(&worker->lock);
	free(worker->DoWorkEventArgs.Argument);
	free(worker->DoWorkEventArgs.Result);
	free(worker);
}


void bw_DoWork(bw_t *worker, void (*DoWork)(bw_t *, bw_DoWorkEventArgs_t *))
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
	worker->WorkerReportsProgress = b;
	pthread_mutex_unlock(&worker->lock);
}


void bw_RunWorkerAsync(bw_t *worker, void *arg, size_t len)
{
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_mutex_lock(&worker->lock);

	if (arg != NULL && len != 0)
	{
		if (worker->DoWorkEventArgs.Argument_len < len)
		{
			void *ptr = realloc(worker->DoWorkEventArgs.Argument, len);
			if (ptr == NULL)
			{
				pthread_mutex_unlock(&worker->lock);
				return;
			}

			worker->DoWorkEventArgs.Argument     = ptr;
			worker->DoWorkEventArgs.Argument_len = len;
		}

		memcpy(worker->DoWorkEventArgs.Argument, arg, len);
	}

	worker->DoWorkEventArgs.Cancel = 0;
	worker->CancellationPending    = 0;

	pthread_create(&worker->tid, &attr, bw_RunWorker, worker);

	pthread_mutex_unlock(&worker->lock);
}

void *bw_RunWorker(void *arg)
{
	void (*DoWork            )(bw_t *, bw_DoWorkEventArgs_t *);
	void (*RunWorkerCompleted)(bw_t *, void *);

	bw_t *worker = (bw_t *) arg;

	pthread_mutex_lock(&worker->lock);
	DoWork = worker->DoWork;
	RunWorkerCompleted = worker->RunWorkerCompleted;
	worker->IsBusy = 1;
	pthread_mutex_unlock(&worker->lock);

	if (DoWork != NULL) DoWork(worker, &worker->DoWorkEventArgs);
	if (RunWorkerCompleted != NULL) RunWorkerCompleted(worker, NULL);

	pthread_mutex_lock(&worker->lock);
	worker->IsBusy = 0;
	pthread_mutex_unlock(&worker->lock);

	pthread_exit(NULL);
}


void bw_ReportProgress(bw_t *worker, int value)
{
	void (*ProgressChanged)(bw_t *, int);

	if (value > 100) value = 100;
	if (value <   0) value =   0;

	pthread_mutex_lock(&worker->lock);
	ProgressChanged = worker->WorkerReportsProgress ? worker->ProgressChanged : NULL;
	pthread_mutex_unlock(&worker->lock);

	if (ProgressChanged != NULL) ProgressChanged(worker, value);
}


/* DoWorkEventArgs functions */

void *bw_DoWorkEventArgs_Argument(bw_DoWorkEventArgs_t *e)
{
	return e->Argument;
}


void bw_DoWorkEventArgs_Result(bw_DoWorkEventArgs_t *e, void *res, size_t len)
{
	if (res == NULL || len == 0) return;

	if (e->Result_len < len)
	{
		void *ptr = realloc(e->Result, len);
		if (ptr == NULL) return;

		e->Result     = ptr;
		e->Result_len = len;
	}

	memcpy(e->Result, res, len);
}
