#ifndef helpers_h_
#define helpers_h_
#include <stdio.h>

void *get_in_addr(struct sockaddr *);
int must_bind_port_fd(int, char *);
bool newline_in_buf(int, char *);
void return_work_file_to_client(FILE *, int);
void write_buf_to_work_file(FILE *, char *);
void sig_handler(int);
bool want_daemon(int, char **);

#endif
