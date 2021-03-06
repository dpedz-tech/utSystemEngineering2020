#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdio.h>
#include "string.h"

typedef int pid_t;
struct lock filesys_lock;

void syscall_init (void);
int sys_write(int fd, const void *buffer, unsigned size);
void sys_halt(void);
int sys_exit(int status);
int sys_exec(const char *cmd_line);
int sys_wait(pid_t pid);
bool sys_create(const char *file, unsigned initial_size);
bool sys_remove(const char *file);
int sys_open(const char *file);
int sys_filesize(int fd);
int sys_read(int fd, void *buffer, unsigned size);
void sys_seek(int fd, unsigned position);
unsigned sys_tell(int fd);
void sys_close(int fd);

#endif /* userprog/syscall.h */
