#include "worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


static double calcPi(int shots, long int seed)
{
	struct drand48_data buffer;
	int hits, shoot;
	double x, y;

	srand48_r(time(NULL) * seed, &buffer);

	for (hits = shoot = 0; shoot < shots; ++shoot)
	{
		drand48_r(&buffer, &x);
		drand48_r(&buffer, &y);
		if (x * x + y * y < 1.0) hits++;
	}

	return 4 * (double) hits / (double) shots;
}

static void calcPi_work(worker_t *worker, void *arg)
{
	int shots = *(int *) arg;
	double pi = calcPi(shots, (long int) worker);
	worker_exit(worker, &pi, sizeof(pi));
}


int main(int argc, char *argv[])
{
	int shots = 10000;
	worker_t *worker[8];
	int w;

	for (w = 0; w < 8; ++w)
	{
		worker[w] = worker_create();
		worker_doWork(worker[w], calcPi_work);
		worker_run(worker[w], &shots, sizeof(shots), WORKER_JOINABLE);
	}

	for (w = 0; w < 8; ++w)
	{
		void *retval;
		double pi;

		worker_join(worker[w], &retval);
		memcpy(&pi, retval, sizeof(pi));
		printf("Pi = %lf\n", pi);

		worker_destroy(worker[w]);
	}

	return 0;
}
