#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "worker.h"


void doWork            (worker_t *worker, void *arg);
void runWorkerCompleted(worker_t *worker, void *res);
void progressChanged   (worker_t *worker, int percentage);


void doWork(worker_t *worker, void *arg)
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

	worker_exit(worker, &pi, sizeof(pi));
}

void runWorkerCompleted(worker_t *worker, void *res)
{
	double pi = *(double *) res;
	printf("Pi = %lf\n", pi);
}

void progressChanged(worker_t *worker, int percentage)
{
	printf(" > progressChanged %d%%\n", percentage);
}


int main(int argc, char *argv[])
{
	int shots = 1000000000;
	worker_t *worker[8];
	int w;

	for (w = 0; w < 8; ++w)
	{
		worker[w] = worker_create();

		worker_doWork(worker[w], doWork);
		worker_runWorkerCompleted(worker[w], runWorkerCompleted);

		worker_runWorkerAsync(worker[w], &shots, sizeof(shots));
	}

	for (w = 0; w < 8; ++w)
	{
		while (worker_isBusy(worker[w]))
		{
			sleep(1);
		}
		worker_destroy(worker[w]);
	}

	return 0;
}
