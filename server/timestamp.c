#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <unistd.h>

#include "timestamp.h"
#include "aesdsocket.h"

void write_timestamp_to_work_file(FILE *fp, struct tm *stamp_time) {
	char buffer[40];
	strftime(buffer, 40, "timestamp:%a %b %d %T %Y\n", stamp_time);

	pthread_mutex_lock(&work_file_lock);
	fseek(fp, 0, SEEK_END);
	fputs(buffer, fp);
	pthread_mutex_unlock(&work_file_lock);
}

void *timestamp_worker(void *ts_void) {
	pthread_t pid = pthread_self();
	fprintf(stderr, "Started timestamp thread with PID %lu\n", pid);

	ts_worker_args ts = *(ts_worker_args *)ts_void;
	struct tm *tm_info;

	while (cease == false) {
		sleep(ts.interval_sec);
		time_t timer = time(NULL);
		tm_info = localtime(&timer);
		if (cease == false) {
			write_timestamp_to_work_file(ts.fp, tm_info);
		}
	}

	return((void *)0);
}