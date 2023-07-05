/*
✅
6.1. Modify your socket based program to accept multiple simultaneous connections, 
     with each connection spawning a new thread to handle the connection.

  ✅ a. Writes to /var/tmp/aesdsocketdata should be synchronized between threads 
        using a mutex, to ensure data written by synchronous connections is not 
        intermixed, and not relying on any file system synchronization.

  ✅ b. The thread should exit when the connection is closed by the client or when 
        an error occurs in the send or receive steps.

  ✅ c. Your program should continue to gracefully exit when SIGTERM/SIGINT is 
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

/*
Make helpers for mutex around linked list.
Make each thread update their own entry in thread tracking list
  when finished before returning.
Can do that by calling function ch_worker_done(&chh, tid) which marks as finished;
Make helper thread that loops over linked list every N seconds and joins finished threads.
*/

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>

#include "aesdsocket.h"
#include "timestamp.h"
#include "helpers.h"

#define TIMESTAMP_INTERVAL 10
#define CH_THREAD_REAP_INTERVAL 1
#define ADDR_BUF_SIZE INET6_ADDRSTRLEN + 1

bool cease = false;
pthread_mutex_t work_file_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ch_thread_status_lock = PTHREAD_MUTEX_INITIALIZER;

struct ch_worker_args {
	FILE *fp;
	char client_addr[ADDR_BUF_SIZE]; 
	int conn_fd;
};

struct ch_entry {
	pthread_t tid;
	bool finished;
	SLIST_ENTRY(ch_entry) ch_entries;
};

SLIST_HEAD(ch_head, ch_entry);
struct ch_head chh;	

void *ch_thread_reaper(void *ch_thread_reaper) {
	pthread_t pid = pthread_self();
	fprintf(stderr, "Started client handler thread reaper with PID %lu\n", pid);

	struct ch_entry *entry, *ch_temp;
	
	while (cease == false) {
		sleep(CH_THREAD_REAP_INTERVAL);

		// walk thread list and cleanup here
		pthread_mutex_lock(&ch_thread_status_lock);

		SLIST_FOREACH_SAFE(entry, &chh, ch_entries, ch_temp) {
			if (entry->finished == true) {
				SLIST_REMOVE(&chh, entry, ch_entry, ch_entries);
				syslog(LOG_USER||LOG_INFO, "CH thread reaper cleaned up %lu", entry->tid);
				free(entry);
			}
		}

		pthread_mutex_unlock(&ch_thread_status_lock);
	}

	return((void *)0);
}

void ch_reaper_new_thread(pthread_t tid) {
	struct ch_entry *ch = malloc(sizeof(struct ch_entry));
	ch->finished = false;
	ch->tid = tid;

	pthread_mutex_lock(&ch_thread_status_lock);
	SLIST_INSERT_HEAD(&chh, ch, ch_entries);
	pthread_mutex_unlock(&ch_thread_status_lock);
}

void ch_reaper_mark_finished() {
	pthread_t tid = pthread_self();
	struct ch_entry *entry;

	pthread_mutex_lock(&ch_thread_status_lock);
	SLIST_FOREACH(entry, &chh, ch_entries) {
		if (entry->tid == tid) {
			entry->finished = true;
			break;
		}
	}
	pthread_mutex_unlock(&ch_thread_status_lock);
}

void *handle_conn(void *ch_void) {
	struct ch_worker_args ch = *(struct ch_worker_args *)ch_void;

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
		bytes_read = recv(ch.conn_fd, recv_buf, NET_BUF_SIZE + 1, 0);

		if (bytes_read <= 0) {
			fprintf(stderr, "read nothing, must be finished\n");
			break;
		}

		recv_buf[bytes_read] = '\0';
		fprintf(stderr, "read %d char: %s\n", bytes_read, recv_buf);

		if (outbuf_size == 0) { // init buf if first time
			outbuf_size = bytes_read;
			out_buf = malloc(outbuf_size + 1);
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
	write_buf_to_work_file(ch.fp, out_buf);
	
	free(out_buf);
	
	return_work_file_to_client(ch.fp, ch.conn_fd);

	close(ch.conn_fd);
	syslog(LOG_USER||LOG_INFO, "Closed connection from %s", ch.client_addr);
	
	ch_reaper_mark_finished();

	free(ch_void);
	pthread_exit((void *)0);
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
	char s[ADDR_BUF_SIZE];

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
	
	// set up for client handler threads
	SLIST_INIT(&chh);

	// TODO: start client handler thread reaper
	pthread_t ch_reaper_tid;
	pthread_create(&ch_reaper_tid, NULL, ch_thread_reaper, NULL);

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

		struct ch_worker_args *wargs = malloc(sizeof(struct ch_worker_args));
		wargs->fp = fp;
		strncpy(wargs->client_addr, s, ADDR_BUF_SIZE);
		wargs->conn_fd = new_fd;
	
		pthread_t ch_tid;
		pthread_create(&ch_tid, NULL, handle_conn, wargs);

		ch_reaper_new_thread(ch_tid);

		syslog(LOG_USER||LOG_INFO, "Handling %s in thread %lu", s, ch_tid);		
		
	}

	// wait for utility threads to cease
	pthread_join(ts_tid, NULL);
	pthread_join(ch_reaper_tid, NULL);
	
	fclose(fp);
	unlink(WORK_FILE);

	return 0;
}
