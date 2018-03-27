#ifndef __BACKGROUNDWORKER_H
#define __BACKGROUNDWORKER_H

#include <stddef.h>

typedef struct backgroundworker bw_t;


/* BackgroundWorker functions */

bw_t *bw_Create(void);
void  bw_Destroy(bw_t *worker);

void  bw_DoWork(bw_t *worker, void (*DoWork)(bw_t *, void *));
void  bw_ProgressChanged(bw_t *worker, void (*ProgressChanged)(bw_t *, int));
void  bw_RunWorkerCompleted(bw_t *worker, void (*RunWorkerCompleted)(bw_t *, void *));

void  bw_CancelAsync(bw_t *worker);
int   bw_CancellationPending(bw_t *worker);

int   bw_IsBusy(bw_t *worker);

void  bw_WorkerReportsProgress(bw_t *worker, int b);

int   bw_RunWorkerAsync(bw_t *worker, void *arg, size_t len);
void  bw_WorkerExit(bw_t *worker, void *res, size_t len);
void  bw_ReportProgress(bw_t *worker, int value);

#endif /* __BACKGROUNDWORKER_H */
