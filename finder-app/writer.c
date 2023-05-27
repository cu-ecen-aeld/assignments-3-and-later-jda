/* given 2 args, first is file, second is string,
   write string to file */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char **argv) {
	openlog(NULL, LOG_PERROR||LOG_PID, LOG_USER);

	if (argc != 3) {
		syslog(LOG_USER||LOG_ERR, "wrong number of args provided. Expected 3, got %d", argc);
		return 1;
	}

	char *file_name = strdup(argv[1]);
	char *output_str = strdup(argv[2]);

	FILE *fp = fopen(file_name, "w");
	if (fp == NULL) {
		char *err_msg = strerror(errno);
		syslog(LOG_USER||LOG_ERR, "could not open %s for writing: %s", file_name, err_msg);
		return 1;
	}

	syslog(LOG_USER||LOG_DEBUG, "Writing %s to %s", output_str, file_name);
	fprintf(fp, "%s", output_str);
	fclose(fp);
	return 0;
}