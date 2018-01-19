#include <stdio.h>
#include <pthread.h>

pthread_mutex_t a_mutex;
pthread_cond_t a_cond;

struct data {
	int num;
};

void *tf1(void *arg) {
	struct data *d = (struct data *)arg;
	pthread_mutex_unlock(&a_mutex);
	printf("Hello from thread #%d. ", d->num);
	fflush(stdout);
	pthread_exit(NULL);
}

int main(int argc, char **argv) {
	int rc, i;
	pthread_t threads[8];
	struct data ds[8];

	pthread_mutex_init(&a_mutex, NULL);

	for (i = 0; i < 8; i++) {
		ds[i].num = i;
		if ((rc = pthread_create(&threads[i], NULL, tf1, &ds[i]))) {
			printf("Something went wrong: %d\n", rc);
		}
		pthread_mutex_lock(&a_mutex);
	}
	printf("\n");
	for (i = 0; i < 8; i++) {
		pthread_join(threads[i], NULL);
		printf("joined with %d. ", i);
	}
	
	return 0;
}
