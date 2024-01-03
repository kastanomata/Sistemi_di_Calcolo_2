#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#define N 10 // number of threads
#define M 10000 // number of iterations per thread
#define V 1 // value added to the balance by each thread at each iteration

unsigned long int shared_variable;
int n = N, m = M, v = V;

void* thread_work(void *arg) {
	int i;
	int *ret = (int *)malloc(sizeof(int));
	for (i = 0; i < m; i++)
		*ret += v;
	printf("\t[Thread] This thread is returning %d\n", *ret);
	return ret;
}

int main(int argc, char **argv)
{
	if (argc > 1) n = atoi(argv[1]);
	if (argc > 2) m = atoi(argv[2]);
	if (argc > 3) v = atoi(argv[3]);
	shared_variable = 0;

	printf("Going to start %d threads, each adding %d times %d to a shared variable initialized to zero...", n, m, v); fflush(stdout);
	pthread_t* threads = (pthread_t*)malloc(n * sizeof(pthread_t)); // also calloc(n,sizeof(pthread_t))
	int i;
	for (i = 0; i < n; i++)
		if (pthread_create(&threads[i], NULL, thread_work, NULL) != 0) {
			fprintf(stderr, "Can't create a new thread, error %d\n", errno);
			exit(EXIT_FAILURE);
		}
	printf("ok\n");

	printf("Waiting for the termination of all the %d threads...\n", n); fflush(stdout);
	for (i = 0; i < n; i++) {
		int *result = malloc(sizeof(int));
		pthread_join(threads[i], (void *)&result);
		printf("Result of the %dith thread= %d\n",i,*result);
		shared_variable += *result;
	}

	printf("ok\n");

	unsigned long int expected_value = (unsigned long int)n*m*v;
	printf("Value of the shared variable: %lu. Expected result: %lu\n", shared_variable, expected_value);
	if (expected_value != shared_variable) {
		unsigned long int difference = (expected_value - shared_variable) / v;
		printf("Number of lost adds: %lu\n", abs(difference));
	}

    free(threads);

	return EXIT_SUCCESS;
}

