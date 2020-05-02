#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "string.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);

struct lock filesys_lock;



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
        sys_tell(*((int *)(f->esp + 4)));
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

 void sys_halt(void) {
  shutdown_power_off();
}
 int sys_exit(int status) {
  struct thread *curThread = thread_current();
  char nameSingle[100] = {0}; char *nameSinglePtr = &nameSingle;
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
  p->exit_status = status;
  thread_exit();
}

 int sys_write(int fd, const void *buffer, unsigned size) {
   int return_code;
  if(0 == fd) {
    return_code = 0;
  } else if(1 == fd) {
    putbuf(buffer, size);
    return_code = size;
  } else if(fd > 1 && fd < 128){
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      lock_acquire (&filesys_lock);
      return_code = ((int)file_write (pFile, buffer, size));
      lock_release (&filesys_lock);
    } else {
      return_code = -1; 
    }
  } else {
    return_code= -1;
  }
  return return_code;
}

 pid_t sys_exec(const char *cmd_line) {
   char * fn_cp = malloc (strlen(cmd_line)+1);
	 strlcpy(fn_cp, cmd_line, strlen(cmd_line)+1);
	  
	 char * save_ptr;
	 fn_cp = strtok_r(fn_cp," ",&save_ptr);

	 struct file* f = filesys_open (fn_cp);

	 if(f==NULL)
	 {
	  	return -1;
	 }
	 else
	 {
	  	file_close(f);
	  	// lock_acquire (&filesys_lock);
	  	
	  	tid_t tid = process_execute(cmd_line);//make this function return a thread structure
	  	//add this thread structure as a list element
	  	// lock_release (&filesys_lock);

	  	return (pid_t) tid;
	  }
}


 int sys_wait(pid_t pid) {
  int rc = process_wait(pid);
  return rc;
}
/*
  Creates a new file called file initially initial size bytes in size.
  Returns true if successful, false otherwise. Creating a new file does
  not open it: opening the new file is a separate operation which would
  require a open system call.
*/
 bool sys_create(const char *file, unsigned initial_size) {
   
  bool return_code;   
  if (strlen(file) > NAME_MAX || strlen(file) == 0) {
    return_code = false;
  } else {
    lock_acquire (&filesys_lock);
    return_code = filesys_create(file, initial_size);
    lock_release (&filesys_lock);
  }
  return return_code;
}
/*
  Deletes the file called file. Returns true if successful, false otherwise. A file may be
  removed regardless of whether it is open or closed, and removing an open file does
  not close it.
*/
 bool sys_remove(const char *file) {
  bool return_code;   
  if(strlen(file) < NAME_MAX || strlen(file) == 0) {
    return_code = false;
  } else {
    lock_acquire (&filesys_lock);
    return_code = filesys_remove(file);
    lock_release (&filesys_lock);
  }
  return return_code;
}

 int sys_open(const char *file) {
  int return_code;
  if (strlen(file) > NAME_MAX || strlen(file) == 0) {
    return_code = -1;
  } else {
    lock_acquire (&filesys_lock);
    struct file *pFile = filesys_open(file);
    lock_release (&filesys_lock);
    if(pFile) {
      return_code = add_fdt_entry(pFile);
    } else {
      return_code = -1; 
    }
  }
  return return_code;
}

 int sys_filesize(int fd) {
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      return ((int)file_length (pFile));
    } else {
      return -1; 
    }
  }
}

 int sys_read(int fd, void *buffer, unsigned size) {
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

void sys_seek (int fd, unsigned position)
{
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      lock_acquire (&filesys_lock);
      file_seek(pFile, position);
      lock_release (&filesys_lock);
    }
  }
}


unsigned sys_tell (int fd)
{ 
  unsigned ret;
  if(fd > 1 && fd < 128) {
    struct file *pFile = thread_current()->fdh.fdt[fd].pFile;
    if(pFile) {
      lock_acquire (&filesys_lock);
      ret= file_tell(pFile);
      lock_release (&filesys_lock);
    }
  }
  return ret;
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



