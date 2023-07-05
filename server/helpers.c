#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <sys/wait.h>

#include "aesdsocket.h"

// get sockaddr no matter if IPv4 or IPv6,
// from https://beej.us/guide/bgnet/examples/server.c
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// returns socket file descriptor on success or exits program on failure.
int must_bind_port_fd(int backlog, char *port_num) {
	// set up socket listener
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints)); // init struct

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;

	int status, sock_fd;
	struct addrinfo *result, *rp;

	if ((status = getaddrinfo(NULL, port_num, &hints, &result)) != 0) {
    	fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(AESD_SOCK_FAIL);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock_fd == -1) {
            continue;
		}

        if (bind(sock_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;                  /* Success */
        }

        close(sock_fd);
	}	

	freeaddrinfo(result);

	if (rp == NULL) {
		char *err_msg = strerror(errno);
		fprintf(stderr, "Could not bind: %s\n", err_msg);
		return -1;
	}

	if ((status = listen(sock_fd, backlog)) != 0) {
		char *err_msg = strerror(errno);
		fprintf(stderr, "Could not listen: %s\n", err_msg);
		exit(AESD_SOCK_FAIL);
	}

	return sock_fd;
}

bool newline_in_buf(int bufsize, char *buf) {
 	for (int i = 0; i< bufsize; i++) {
 		if (buf[i] == '\n') {
 			return true;
 		}
 	}
	return false;
}

void return_work_file_to_client(FILE *fp, int conn_fd) {
	char buffer[NET_BUF_SIZE];

	pthread_mutex_lock(&work_file_lock);
	fseek(fp, 0, SEEK_SET);
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		write(conn_fd, buffer, strlen(buffer));
	}
	pthread_mutex_unlock(&work_file_lock);
}

void write_buf_to_work_file(FILE *fp, char *out_buf) {
	pthread_mutex_lock(&work_file_lock);
	fseek(fp, 0, SEEK_END);
	fputs(out_buf, fp);
	pthread_mutex_unlock(&work_file_lock);
}

void sig_handler(int s) {
	syslog(LOG_USER||LOG_INFO, "Caught signal, exiting");
	cease = true;

	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

bool want_daemon(int argc, char **argv) {
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0) {
			printf("want daemon\n");
			return true;
		}
	}

	return false;
}