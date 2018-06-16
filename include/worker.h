#ifndef __WORKER_H
#define __WORKER_H

#include <stddef.h>


typedef struct worker worker_t;

enum {
	WORKER_DETACHED,
	WORKER_JOINABLE
};


worker_t *worker_create(void);
void worker_destroy(worker_t *worker);

void worker_doWork(worker_t *worker, void (*doWork)(worker_t *, void *));
void worker_progressChanged(worker_t *worker, void (*progressChanged)(worker_t *, int));
void worker_runWorkerCompleted(worker_t *worker, void (*runWorkerCompleted)(worker_t *, void *));

void worker_cancelAsync(worker_t *worker);
void worker_reportProgress(worker_t *worker, int value);
int  worker_cancellationPending(worker_t *worker);
int  worker_isBusy(worker_t *worker);

int  worker_run(worker_t *worker, void *arg, size_t len, int detachstate);
void worker_exit(worker_t *worker, void *res, size_t len) __attribute__((__noreturn__));
int  worker_join(worker_t *worker, void **retval);
int  worker_kill(worker_t *worker, int signo);

#endif /* __WORKER_H */
