#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "string.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);

static bool
usr_ptr_check(const void *ptr) {
  if(!ptr || !is_user_vaddr(ptr) || NULL == pagedir_get_page(thread_current()->pagedir, ptr)) {
    return false;
  } else {
    return true;
  }
}

static bool
str_check(const char *str) {
  for(int str_idx = 0; true == usr_ptr_check((void *)(str + str_idx)); str_idx++) {
      if(str[str_idx] == 0) {
        return true;
      }
  }
  return false;
}

static bool
buf_check(const void *buffer, unsigned size) {
  for(unsigned buf_idx = 0; true == usr_ptr_check(buffer + buf_idx); buf_idx++) {
    if((0 == size) || (buf_idx == size - 1)) {
      return true;
    }
  }
  return false;
}

static int
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
  lock_init (&filesys_lock);
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
        lock_acquire (&filesys_lock);
        f->eax = sys_exec(*(char **)(f->esp + 4));
        lock_release (&filesys_lock);
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
        lock_acquire (&filesys_lock);
        f->eax = sys_create((*(char **)(f->esp + 4)), (*(unsigned *)(f->esp + 8)));
        lock_release (&filesys_lock);
      }
      break;
    case SYS_REMOVE:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == str_check(*(char **)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        lock_acquire (&filesys_lock);
        f->eax = sys_remove(*(char **)(f->esp + 4));
        lock_release (&filesys_lock);
      }
      break;
    case SYS_OPEN:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == str_check(*(char **)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        lock_acquire (&filesys_lock);
        f->eax = sys_open((*(const char **)(f->esp + 4)));
        lock_release (&filesys_lock);
      }
      break;
    case SYS_FILESIZE:
      if(false == usr_ptr_check((void *)(f->esp + 4))) {
        sys_exit(-1);
      } else {
        lock_acquire (&filesys_lock);
        f->eax = sys_filesize(*((int *)(f->esp + 4)));
        lock_release (&filesys_lock);
      }
      break;
    case SYS_READ:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == usr_ptr_check((void *)(f->esp + 8)))
      || (false == usr_ptr_check((void *)(f->esp + 12)))
      || (false == buf_check(*(void **)(f->esp + 8), *(unsigned *)(f->esp + 12)))) {
        sys_exit(-1);
      } else {
        lock_acquire (&filesys_lock);
        f->eax = sys_read((*(int *)(f->esp + 4)), (*(void **)(f->esp + 8)), (*(unsigned *)(f->esp + 12)));
        lock_release (&filesys_lock);
      }
      break;
    case SYS_WRITE:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == usr_ptr_check((void *)(f->esp + 8)))
      || (false == usr_ptr_check((void *)(f->esp + 12)))
      || (false == buf_check(*(void **)(f->esp + 8), *(unsigned *)(f->esp + 12)))) {
        sys_exit(-1);
      } else {
        lock_acquire (&filesys_lock);
        f->eax = sys_write((*(int *)(f->esp + 4)), (*(void **)(f->esp + 8)), (*(unsigned *)(f->esp + 12)));
        lock_release (&filesys_lock);
      }
      break;
    case SYS_SEEK:
      if((false == usr_ptr_check((void *)(f->esp + 4)))
      || (false == usr_ptr_check((void *)(f->esp + 8)))) {
        sys_exit(-1);
      } else {
        lock_acquire (&filesys_lock);
        sys_seek(*((int *)(f->esp + 4)), *((unsigned *)(f->esp + 8)));
        lock_release (&filesys_lock);
      }
      break;
    case SYS_TELL:
      if((false == usr_ptr_check((void *)(f->esp + 4)))) {
        sys_exit(-1);
      } else {
        lock_acquire (&filesys_lock);
        sys_tell(*((int *)(f->esp + 4)));
        lock_release (&filesys_lock);
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
/*
  Terminates Pintos by calling shutdown_power_off() (declared in
  ‘devices/shutdown.h’). This should be seldom used, because you lose
  some information about possible deadlock situations, etc.
 */
void sys_halt(void) {
  shutdown_power_off();
}
/*
  Terminates the current user program, returning status to the kernel. If the process’s
  parent waits for it (see below), this is the status that will be returned. Conventionally,
  a status of 0 indicates success and nonzero values indicate errors.
 */
int sys_exit(int status) {
  struct thread *curThread = thread_current();
  char nameSingle[100] = {0}; char *nameSinglePtr = (char *) &nameSingle;
  strlcpy(nameSingle, curThread->name, strlen(curThread->name) + 1);
  printf("%s: exit(%d)\n",strtok_r(nameSingle, " ", &nameSinglePtr),status);
  for(int i = 0; i < 128; i++) {
    sys_close(i);
  }
  if(curThread->file_to_exe) {
    file_close(curThread->file_to_exe);
    curThread->file_to_exe = NULL;
  }
  sProcType *p = fetch_process(curThread->tid);
  if(p) {
    p->exit_status = status;
  }
  thread_exit();
}

 int sys_write(int fd, const void *buffer, unsigned size) {
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

 pid_t sys_exec(const char *cmd_line) {
   char * fn_cp = malloc (strlen(cmd_line)+1);
	 strlcpy(fn_cp, cmd_line, strlen(cmd_line)+1);
	  
	 char * save_ptr;
	 fn_cp = strtok_r(fn_cp," ",&save_ptr);

	 struct file* f = filesys_open (fn_cp);

	 if(!f) {
	  	return -1;
	 } else {
	  	file_close(f);
	  	return ((pid_t) process_execute(cmd_line));
	  }
}


 int sys_wait(pid_t pid) {
  return process_wait(pid);
}
/*
  Creates a new file called file initially initial size bytes in size.
  Returns true if successful, false otherwise. Creating a new file does
  not open it: opening the new file is a separate operation which would
  require a open system call.
*/
 bool sys_create(const char *file, unsigned initial_size) {
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
 bool sys_remove(const char *file) {
  if(strlen(file) > NAME_MAX || strlen(file) == 0) {
    return false;
  } else {
    return filesys_remove(file);
  }
}

 int sys_open(const char *file) {
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

 int sys_filesize(int fd) {
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      return ((int)file_length (pFile));
    } else {
      return -1; 
    }
  } else {
    return -1;
  }
}

 int sys_read(int fd, void *buffer, unsigned size) {
  if(1 == fd) {
    return 0;
  } else if(0 == fd) {
    char *buffer_ch = (char *) buffer;
    for(unsigned i = 0; i < size; i++) {
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

void sys_seek (int fd, unsigned position)
{
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      file_seek(pFile, position);
    }
  }
}


unsigned sys_tell (int fd)
{ 
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      return ((unsigned) file_tell(pFile));
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

 void sys_close(int fd) {
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      file_close(pFile);
      thread_current()->fdh.fdt[fd].pFile = NULL;
    }
  }
}