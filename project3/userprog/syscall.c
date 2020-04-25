#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "userprog/pagedir.h"

typedef int pid_t;

static void syscall_handler (struct intr_frame *);
static int sys_write(int fd, const void *buffer, unsigned size);
static void sys_halt(void);
static int sys_exit(int status);
static int sys_exec(const char *cmd_line);
static int sys_wait(pid_t pid);
static bool sys_create(const char *file, unsigned initial_size);
static bool sys_remove(const char *file);
static int sys_open(const char *file);
static int sys_filesize(int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);


bool
usr_ptr_check(const void *ptr) {
  if(!ptr || !is_user_vaddr(ptr) || NULL == pagedir_get_page(thread_current()->pagedir, ptr)) {
    return false;
  } else {
    return true;
  }
}

bool
str_check(const char *str) {
  for(int str_idx = 0; true == usr_ptr_check((void *)(str + str_idx)); str_idx++) {
      if(str[str_idx] == 0) {
        return true;
      }
  }
  return false;
}

bool
buf_check(const void *buffer, unsigned size) {
  for(unsigned buf_idx = 0; true == usr_ptr_check(buffer + buf_idx); buf_idx++) {
    if((0 == size) || (buf_idx == size - 1)) {
      return true;
    }
  }
  return false;
}

int
add_fdt_entry(struct file *pFile) {
  struct thread *thr = thread_current();
  for(int i = 2; i < 128; i++) {
    if(NULL == thr->fdh.fdt[i].pFile) {
      thr->fdh.fdt[i].pFile = pFile;
      return i;
    }
  }
  return -1;
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  if(false == usr_ptr_check(f->esp)) {
    sys_exit(-1);
  }
  switch((*(uint32_t *)f->esp)) {
    case SYS_HALT:
      sys_halt();
      break;
    case SYS_EXIT:
      if(false == usr_ptr_check((void *)f->esp + 4)) {
        sys_exit(-1);
      } else {
        f->eax = sys_exit((*(int *)(f->esp + 4)));
      }
      break;
    case SYS_EXEC:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == str_check(*(char **)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        f->eax = sys_exec(*(char **)(f->esp + 4));
      }
      break;
    case SYS_WAIT:
      if((false == usr_ptr_check((void *)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        f->eax = sys_wait(*((pid_t *)(f->esp +4)));
      }
      break;
    case SYS_CREATE:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == usr_ptr_check((void *)(f->esp + 8)))
      || (false == str_check(*(char **)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        f->eax = sys_create((*(char **)(f->esp + 4)), (*(unsigned *)(f->esp + 8)));
      }
      break;
    case SYS_REMOVE:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == str_check(*(char **)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        f->eax = sys_remove(*(char **)(f->esp + 4));
      }
      break;
    case SYS_OPEN:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == str_check(*(char **)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        f->eax = sys_open((*(const char **)(f->esp + 4)));
      }
      break;
    case SYS_FILESIZE:
      if(false == usr_ptr_check((void *)(f->esp + 4))) {
        sys_exit(-1);
      } else {
        f->eax = sys_filesize(*((int *)(f->esp + 4)));
      }
      break;
    case SYS_READ:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == usr_ptr_check((void *)(f->esp + 8)))
      || (false == usr_ptr_check((void *)(f->esp + 12)))
      || (false == buf_check(*(void **)(f->esp + 8), *(unsigned *)(f->esp + 12)))) {
        sys_exit(-1);
      } else {
        f->eax = sys_read((*(int *)(f->esp + 4)), (*(void **)(f->esp + 8)), (*(unsigned *)(f->esp + 12)));
      }
      break;
    case SYS_WRITE:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == usr_ptr_check((void *)(f->esp + 8)))
      || (false == usr_ptr_check((void *)(f->esp + 12)))
      || (false == buf_check(*(void **)(f->esp + 8), *(unsigned *)(f->esp + 12)))) {
        sys_exit(-1);
      } else {
        f->eax = sys_write((*(int *)(f->esp + 4)), (*(void **)(f->esp + 8)), (*(unsigned *)(f->esp + 12)));
      }
      break;
    case SYS_SEEK:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == usr_ptr_check((void *)(f->esp + 8)))) {
        sys_exit(-1);
      } else {
        sys_seek(*((int *)(f->esp + 4)), *((unsigned *)(f->esp + 8)));
      }
      break;
    case SYS_TELL:
      if((false == usr_ptr_check((void *)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        f->eax = sys_tell(*((int *)(f->esp + 4)));
      }
      break;
    case SYS_CLOSE:
      if((false == usr_ptr_check((void *)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        sys_close(*((int *)(f->esp + 4)));
      }
      break;
    default:
      printf("Unknown Sys Call!");
      thread_exit();
  }
}

static void sys_halt(void) {
  shutdown_power_off();
}
static int sys_exit(int status) {
  struct thread *curThread = thread_current();
  char nameSingle[100] = {0}; char *nameSinglePtr = &nameSingle;
  strlcpy(nameSingle, curThread->name, strlen(curThread->name) + 1);
  printf("%s: exit(%d)\n",strtok_r(nameSingle, " ", &nameSinglePtr),status);
  thread_exit();
}

static int sys_write(int fd, const void *buffer, unsigned size) {
  if(0 == fd) {
    return 0;
  } else if(1 == fd) {
    putbuf(buffer, size);
    return size;
  } else if(fd > 1 && fd < 128){
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      return ((int)file_write (pFile, buffer, size));
    } else {
      return -1; 
    }
  } else {
    return -1;
  }
}

static pid_t sys_exec(const char *cmd_line) {
  return -1;
}

static int sys_wait(pid_t pid) {
  return -1;
}
/*
  Creates a new file called file initially initial size bytes in size.
  Returns true if successful, false otherwise. Creating a new file does
  not open it: opening the new file is a separate operation which would
  require a open system call.
*/
static bool sys_create(const char *file, unsigned initial_size) {
  if (strlen(file) > NAME_MAX || strlen(file) == 0) {
    return false;
  } else {
    return filesys_create(file, initial_size);
  }
}
/*
  Deletes the file called file. Returns true if successful, false otherwise. A file may be
  removed regardless of whether it is open or closed, and removing an open file does
  not close it.
*/
static bool sys_remove(const char *file) {
  if(strlen(file) < NAME_MAX || strlen(file) == 0) {
    return false;
  } else {
    return filesys_remove(file);
  }
}

static int sys_open(const char *file) {
  if (strlen(file) > NAME_MAX || strlen(file) == 0) {
    return -1;
  } else {
    struct file *pFile = filesys_open(file);
    if(pFile) {
      return add_fdt_entry(pFile);
    } else {
      return -1; 
    }
  }
}
static int sys_filesize(int fd) {
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      return ((int)file_length (pFile));
    } else {
      return -1; 
    }
  }
}

static int sys_read(int fd, void *buffer, unsigned size) {
  if(1 == fd) {
    return 0;
  } else if(0 == fd) {
    char *buffer_ch = (char *) buffer;
    for(int i = 0; i < size; i++) {
      buffer_ch[i] = input_getc();
    }
    return size;
  } else if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      return ((int)file_read (pFile, buffer, size));
    } else {
      return -1; 
    }
  } else {
    return -1;
  }
}

static void sys_seek(int fd, unsigned position) {
  
}

static unsigned sys_tell(int fd) {
  return 0;
}

static void sys_close(int fd) {
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      file_close(pFile);
      thread_current()->fdh.fdt[fd].pFile = NULL;
    }
  }
}
