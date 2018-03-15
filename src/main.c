#include <stdio.h>
#include <unistd.h>

#include "backgroundworker.h"


void DoWork            (bw_t *worker, bw_DoWorkEventArgs_t *e);
void ProgressChanged   (bw_t *worker, int);
void RunWorkerCompleted(bw_t *worker, void *arg);

void DoWork(bw_t *worker, bw_DoWorkEventArgs_t *e)
{
	int loops = *(int *) bw_DoWorkEventArgs_Argument(e);
	char res[] = "MABITE\n";
	int i;

	for (i = 0; i < loops; ++i)
	{
		printf("DoWork loop %d\n", i);
		bw_ReportProgress(worker, i * 100 / loops);
	}

	bw_ReportProgress(worker, 100);

	bw_DoWorkEventArgs_Result(e, res, sizeof(res));
}

void ProgressChanged(bw_t *worker, int percentage)
{
	printf(" > ProgressChanged %d%%\n", percentage);
}

void RunWorkerCompleted(bw_t *worker, void *arg)
{
	printf("RunWorkerCompleted\n");
}

int main(int argc, char *argv[])
{
	bw_t *worker[3];
	int loops = 12;
	int w;

	for (w = 0; w < 3; ++w)
	{
		worker[w] = bw_Create();

		bw_DoWork(worker[w], DoWork);
		bw_ProgressChanged(worker[w], ProgressChanged);
		bw_RunWorkerCompleted(worker[w], RunWorkerCompleted);
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
