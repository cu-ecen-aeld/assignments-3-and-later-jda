/*
2. Create a socket based program with name aesdsocket in the “server” directory which:

  ✅ a. Is compiled by the “all” and “default” target of a Makefile in the “server” 
        directory and supports cross compilation, placing the executable file in the “server” 
        directory and named aesdsocket.

  ✅ b. Opens a stream socket bound to port 9000, failing and returning -1 if any of the 
        socket connection steps fail.

  ✅ c. Listens for and accepts a connection

  ✅ d. Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address 
        of the connected client. 

  ✅  e. Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating 
        this file if it doesn’t exist.

Your implementation should use a newline to separate data packets received.
  In other words a packet is considered complete when a newline character is found in 
  the input receive stream, and each newline should result in an append to the 
  /var/tmp/aesdsocketdata file.

You may assume the data stream does not include null characters (therefore can be processed 
  using string handling functions).

You may assume the length of the packet will be shorter than the available heap size.
  In other words, as long as you handle malloc() associated failures with error messages 
  you may discard associated over-length packets.

   ✅  f. Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the 
        received data packet completes.

You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) 
  will be less than the size of the root filesystem, however you may not assume this total 
  size of all packets sent will be less than the size of the available RAM for the process heap.

   ✅  g. Logs message to the syslog “Closed connection from XXX” where XXX is the IP address 
        of the connected client.

   ✅  h. Restarts accepting connections from new clients forever in a loop until SIGINT or 
        SIGTERM is received (see below).

   ✅  i. Gracefully exits when SIGINT or SIGTERM is received, completing any open connection 
        operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.

Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.

3. Install the netcat utility on your Ubuntu development system using sudo apt-get install netcat

4. Verify the sample test script `sockettest.sh` successfully completes against your native 
   compiled application each time your application is closed and restarted.
   You can run this manually outside the ./full-test.sh script by:
   * Starting your aesdsocket application
   * Executing the sockettest.sh script from the assignment-autotest subdirectory.
   * Stopping your aesdsocket application.

5. Modify your program to support a -d argument which runs the aesdsocket application as a 
   daemon. When in daemon mode the program should fork after ensuring it can bind to port 9000.

You can now verify that the ./full-test.sh script from your aesd-assignments repository 
  successfully verifies your socket application running as a daemon.
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

#define PORT_NUM "9000"
#define BACKLOG 20
#define AESD_SOCK_FAIL -1
#define WORK_FILE "/var/tmp/aesdsocketdata"
#define NET_BUF_SIZE 1000

bool cease = false; // when true, wrap up the listen loop.

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
	fseek(fp, 0, SEEK_SET);

	char buffer[NET_BUF_SIZE];

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		write(conn_fd, buffer, strlen(buffer));
		//printf("read: %s\n", buffer);
	}


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

		if (newline_in_buf(outbuf_size, out_buf) == true) {
			break;
		}


	}
	free(recv_buf);

	// flush to file
	fprintf(stderr, "Got stuff: %s\n", out_buf);
	fseek(fp, 0, SEEK_END);
	fputs(out_buf, fp);
	
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

int main(int argc, char **argv) {
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