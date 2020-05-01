#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"


typedef struct sSemType{
    struct semaphore launched;
    struct semaphore exiting;
    int present;
}sSemType;

typedef struct sProcType{
    struct list_elem elem;

    tid_t tid;
    tid_t parent_tid;
    int sem_list_idx;
    int exit_status;
    struct list child_list;
}sProcType;

sProcType *fetch_process(tid_t tid);
void process_init(void);
tid_t process_execute (const char *file_name);
tid_t child_process_execute(const char *command);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
