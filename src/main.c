#include <stdio.h>
#include <unistd.h>

#include "backgroundworker.h"


void DoWork            (bw_t *worker, void *arg);
void RunWorkerCompleted(bw_t *worker, void *res);
void ProgressChanged   (bw_t *worker, int percentage);

void DoWork(bw_t *worker, void *arg)
{
	int loops = *(int *) arg;
	char res[] = "TASK OVER";
	int i;

	for (i = 0; i < loops; ++i)
	{
		printf("DoWork loop %d\n", i);
		bw_ReportProgress(worker, (i + 1) * 100 / loops);
	}

	bw_WorkerComplete(worker, res, sizeof(res));
}

void RunWorkerCompleted(bw_t *worker, void *res)
{
	char *str = (char *) res;
	printf("RunWorkerCompleted %s\n", str);
}

void ProgressChanged(bw_t *worker, int percentage)
{
	printf(" > ProgressChanged %d%%\n", percentage);
}


int main(int argc, char *argv[])
{
	bw_t *worker[3];
	int loops = 10;
	int w;

	for (w = 0; w < 3; ++w)
	{
		worker[w] = bw_Create();

		bw_DoWork(worker[w], DoWork);
		bw_RunWorkerCompleted(worker[w], RunWorkerCompleted);
		bw_ProgressChanged(worker[w], ProgressChanged);
		bw_WorkerReportsProgress(worker[w], 1);

		bw_RunWorkerAsync(worker[w], &loops, sizeof(loops));
	}

	sleep(1);

	for (w = 0; w < 3; ++w)
	{
		bw_Destroy(worker[w]);
	}

	return 0;
}
