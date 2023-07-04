#ifndef timestamp_h_
#define timestamp_h_
#include <stdio.h>

typedef struct ts_worker_args {
	FILE *fp;
	int interval_sec;
} ts_worker_args;

void *timestamp_worker(void *);
#endif