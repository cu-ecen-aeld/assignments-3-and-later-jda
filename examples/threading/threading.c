#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void *thread_param)
{
    struct thread_data* td = (struct thread_data *) thread_param;

    usleep(td->wait_to_obtain_ms * 1000);
    pthread_mutex_lock(td->mutex);
    
    usleep(td->wait_to_release_ms * 1000);
    pthread_mutex_unlock(td->mutex);

    td->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(
    pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, 
    int wait_to_release_ms)
{
    struct thread_data *td = (struct thread_data*)malloc(sizeof(struct thread_data));
    
    td->wait_to_obtain_ms = wait_to_obtain_ms;
    td->wait_to_release_ms = wait_to_release_ms;
    td->mutex = mutex;

    int pt_status = pthread_create(thread, NULL, threadfunc, td);
    if (pt_status == 0) {
        return true;
    }

    return false;
}

