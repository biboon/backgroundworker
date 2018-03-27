#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "backgroundworker.h"


void DoWork            (bw_t *worker, void *arg);
void RunWorkerCompleted(bw_t *worker, void *res);
void ProgressChanged   (bw_t *worker, int percentage);


void DoWork(bw_t *worker, void *arg)
{
	struct drand48_data buffer;
	double x, y, pi;
	int shots = *(int *) arg;
	int hits = 0;
	int shoot;

	srand48_r(time(NULL) * (long int) worker, &buffer);

	for (shoot = 0; shoot < shots; ++shoot)
	{
		drand48_r(&buffer, &x);
		drand48_r(&buffer, &y);
		if (x * x + y * y < 1.0) hits++;
	}

	pi = 4 * (double) hits / (double) shots;

	bw_WorkerExit(worker, &pi, sizeof(pi));
}

void RunWorkerCompleted(bw_t *worker, void *res)
{
	double pi = *(double *) res;
	printf("Pi = %lf\n", pi);
}

void ProgressChanged(bw_t *worker, int percentage)
{
	printf(" > ProgressChanged %d%%\n", percentage);
}


int main(int argc, char *argv[])
{
	int shots = 1000000000;
	bw_t *worker[8];
	int w;

	for (w = 0; w < 8; ++w)
	{
		worker[w] = bw_Create();

		bw_DoWork(worker[w], DoWork);
		bw_RunWorkerCompleted(worker[w], RunWorkerCompleted);

		bw_RunWorkerAsync(worker[w], &shots, sizeof(shots));
	}

	for (w = 0; w < 8; ++w)
	{
		while (bw_IsBusy(worker[w]))
		{
			sleep(1);
		}
		bw_Destroy(worker[w]);
	}

	return 0;
}
