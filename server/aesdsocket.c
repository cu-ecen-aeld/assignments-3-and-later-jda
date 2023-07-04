/*
✅
6.1. Modify your socket based program to accept multiple simultaneous connections, 
     with each connection spawning a new thread to handle the connection.

     a. Writes to /var/tmp/aesdsocketdata should be synchronized between threads 
        using a mutex, to ensure data written by synchronous connections is not 
        intermixed, and not relying on any file system synchronization.

     b. The thread should exit when the connection is closed by the client or when 
        an error occurs in the send or receive steps.

     c. Your program should continue to gracefully exit when SIGTERM/SIGINT is 
        received, after requesting an exit from each thread and waiting for threads 
        to complete execution.

     d. Use the singly linked list APIs discussed in the video (or your own 
        implementation if you prefer) to manage threads.

6.2. Modify your aesdsocket source code repository to:

  ✅ a. Append a timestamp in the form “timestamp:time” where time is specified by the 
        RFC 2822 compliant strftime format, followed by newline.  This string 
        should be appended to the /var/tmp/aesdsocketdata file every 10 seconds, where 
        the string includes the year, month, day, hour (in 24 hour format) minute and 
        second representing the system wall clock time.

  ✅ b. Use appropriate locking to ensure the timestamp is written atomically with
        respect to socket data

*/

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#define PORT_NUM "9000"
#define BACKLOG 20
#define AESD_SOCK_FAIL -1
#define WORK_FILE "/var/tmp/aesdsocketdata"
#define NET_BUF_SIZE 1000
#define TIMESTAMP_INTERVAL 10

bool cease = false; // when true, wrap up the listen loop.
pthread_mutex_t work_file_lock = PTHREAD_MUTEX_INITIALIZER;

struct ts_worker_args {
	FILE *fp;
	int interval_sec;
};

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
		if (sock_fd == -1)
            continue;

        if (bind(sock_fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(sock_fd);
	}	

	freeaddrinfo(result);

	if (rp == NULL) {
		char *err_msg = strerror(errno);
		fprintf(stderr, "Could not bind: %s\n", err_msg);
		exit(AESD_SOCK_FAIL);
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
		//printf("read: %s\n", buffer);
	}
	pthread_mutex_unlock(&work_file_lock);
}

void write_buf_to_work_file(FILE *fp, char *out_buf) {
	pthread_mutex_lock(&work_file_lock);
	fseek(fp, 0, SEEK_END);
	fputs(out_buf, fp);
	pthread_mutex_unlock(&work_file_lock);
}

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

	struct ts_worker_args ts = *(struct ts_worker_args *)ts_void;
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

/*
  copy from conn_fd to fp until \n, 
  then copy all of fp to conn_fd
  and then log and close the conn;
*/
void handle_conn(FILE *fp, char client_addr[INET6_ADDRSTRLEN], int conn_fd) {
	char *out_buf; // what we'll write to file
	char *recv_buf; // what we're working with while reading from sock

	recv_buf = malloc(NET_BUF_SIZE + 1);
	if (recv_buf == NULL) {
		char *err_msg = strerror(errno);
		fprintf(stderr, "Could not alloc mem for recv buffer: %s\n", err_msg);
		exit(EXIT_FAILURE);
	}

	int bytes_read = 0;
	int outbuf_size = 0;
	while (true) {
		bytes_read = recv(conn_fd, recv_buf, NET_BUF_SIZE + 1, 0);

		if (bytes_read <= 0) {
			fprintf(stderr, "read nothing, must be finished\n");
			break;
		}

		recv_buf[bytes_read] = '\0';
		fprintf(stderr, "read %d char: %s\n", bytes_read, recv_buf);

		if (outbuf_size == 0) { // init buf if first time
			outbuf_size = bytes_read;
			out_buf = malloc(outbuf_size);
			if (out_buf == NULL) {
				char *err_msg = strerror(errno);
				fprintf(stderr, "Could not alloc mem for out buffer: %s\n", err_msg);
				exit(EXIT_FAILURE);
			}

			strcpy(out_buf, recv_buf);
		} else { // time to grow outbuf!
			out_buf = realloc(out_buf, outbuf_size + bytes_read);
			strcpy(out_buf + strlen(out_buf), recv_buf);
		}
		outbuf_size += bytes_read;

		if (newline_in_buf(bytes_read, recv_buf) == true) {
			fprintf(stderr, "found newline, done with this\n");
			break;
		}


	}
	free(recv_buf);

	// flush to file
	fprintf(stderr, "Got stuff: %s\n", out_buf);
	write_buf_to_work_file(fp, out_buf);
	
	free(out_buf);
	
	return_work_file_to_client(fp, conn_fd);

	close(conn_fd);
	syslog(LOG_USER||LOG_INFO, "Closed connection from %s", client_addr);
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

int main(int argc, char **argv) {
	if (want_daemon(argc, argv) == true) {
		daemon(0,0);
	}
	fprintf(stderr, "ready to work!\n");

	// setup syslog
	openlog(NULL, LOG_PERROR||LOG_PID, LOG_USER);

	int sock_fd = must_bind_port_fd(BACKLOG, PORT_NUM);


	int new_fd;
	struct sockaddr_storage their_addr; // client addr
	socklen_t sin_size;
	char s[INET6_ADDRSTRLEN];

	FILE *fp = fopen(WORK_FILE, "a+");
	if (fp == NULL) {
		char *err_msg = strerror(errno);
		fprintf(stderr, "Could not open work file: %s\n", err_msg);
		exit(EXIT_FAILURE);
	}

	// SIGINT or SIGTERM 
	struct sigaction sa = {.sa_handler = sig_handler};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	// start timestamp thread
	pthread_t ts_tid;
	struct ts_worker_args tsa = {.fp = fp, .interval_sec = TIMESTAMP_INTERVAL};
	pthread_create(&ts_tid, NULL, timestamp_worker, &tsa);
	pthread_join(ts_tid, NULL);

	// accept loop
	while(cease == false) {
		sin_size = sizeof their_addr;
		new_fd = accept(sock_fd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);

		syslog(LOG_USER||LOG_INFO, "Accepted connection from %s", s);

		if (!fork()) { 
			close(sock_fd); // child doesn't need the listener
			handle_conn(fp, s, new_fd);
			close(new_fd);
			exit(0);
		}
	}

	fclose(fp);
	unlink(WORK_FILE);

	return 0;
}