#ifndef aesdsocket_h_
#define aesdsocket_h_

#define PORT_NUM "9000"
#define BACKLOG 20
#define AESD_SOCK_FAIL -1
#define WORK_FILE "/var/tmp/aesdsocketdata"
#define NET_BUF_SIZE 1000

extern bool cease; // flag for threads to quit
extern pthread_mutex_t work_file_lock;

#endif