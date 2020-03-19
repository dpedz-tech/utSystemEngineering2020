/*
 * File Name: yash.c
 * Author: Alex Hoganson
 * Description: File contains main method for yash executable
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <pty.h>
#include <arpa/inet.h>
#include "yash.h"
#include "yashd.h"
#include "yash_thread.h"

#define REDIR_FILE_MASK O_TRUNC | O_WRONLY | O_CREAT
#define FILEPERMISSIONS S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH

void debug_logger(const char* format, ...);

void get_fg_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	memcpy(cmdInfo, &(yash_thread_info->fgInfo), sizeof(sCommandDetail));
	sem_post(job_sem);
}
void push_fg_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	memcpy(&(yash_thread_info->fgInfo), cmdInfo, sizeof(sCommandDetail));
	sem_post(job_sem);
}
void pop_fg_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	memset(&(yash_thread_info->fgInfo), 0, sizeof(sCommandDetail));
	sem_post(job_sem);
}
void push_job_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	memcpy(&(yash_thread_info->jobs[yash_thread_info->jobsNextIndex]), cmdInfo, sizeof(sCommandDetail));
    yash_thread_info->jobsNextIndex++;
	sem_post(job_sem);
}
void update_job_cmd(sYashThreadType *yash_thread_info, pid_t pgId, EProcStatus status) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	for(int i = 0; i < MAX_JOBS; i++) {
		if(pgId == yash_thread_info->jobs[i].jobInfo.pgId) {
			yash_thread_info->jobs[i].jobInfo.status = status;
			break;
		}
	}
	sem_post(job_sem);
}
void remove_job_cmd(sYashThreadType *yash_thread_info, pid_t pgId) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	for(int i = 0; i < MAX_JOBS; i++) {
		if(pgId == yash_thread_info->jobs[i].jobInfo.pgId) {
			memset(&(yash_thread_info->jobs[i]), 0, sizeof(sCommandDetail));
			if((yash_thread_info->jobsNextIndex - 1) == i) {
            	while((yash_thread_info->jobsNextIndex)
            	&& 	!(yash_thread_info->jobs[yash_thread_info->jobsNextIndex - 1].jobInfo.pgId)) {
               		yash_thread_info->jobsNextIndex--;
            	} 
        	}
        	break;
		}
	}
	sem_post(job_sem);
}
void get_last_job_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	if(0 < yash_thread_info->jobsNextIndex) {
		memcpy(cmdInfo, &(yash_thread_info->jobs[yash_thread_info->jobsNextIndex - 1]), sizeof(sCommandDetail));
	}
	sem_post(job_sem);
}
void get_last_stop_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	for(int i = yash_thread_info->jobsNextIndex - 1; i >= 0; i--) {
		if((yash_thread_info->jobs[i].jobInfo.pgId)
		&&(PROC_STOP == yash_thread_info->jobs[i].jobInfo.status)) {
			memcpy(cmdInfo, &(yash_thread_info->jobs[i]), sizeof(sCommandDetail));
			break;
		}
	}
	sem_post(job_sem);
}

void print_cmd_info(sCommandDetail *cmdInfo) {
	debug_logger("[%s]: Here",__func__);
    for(int cmdIndex = 0; cmdIndex < MAX_PIPE_SIZE; cmdIndex++) {
        if(1 < cmdInfo->cmdMatrix[cmdIndex].argvSize) {
            int argvSize = cmdInfo->cmdMatrix[cmdIndex].argvSize;
            debug_logger("argV Size: %d",argvSize);
            for(int argIndex = 0; argIndex < argvSize; argIndex++) {
                if(cmdInfo->cmdMatrix[cmdIndex].argv[argIndex]) {
                    debug_logger("argV[%d]: %s, argVExec: %p",argIndex, cmdInfo->cmdMatrix[cmdIndex].argv[argIndex], cmdInfo->cmdMatrix[cmdIndex].argvExec[argIndex]);
                }
            }
            debug_logger("Std Err Redirect: %d, File: %s",
                        cmdInfo->cmdMatrix[cmdIndex].isStdErrRedirect,
                        cmdInfo->cmdMatrix[cmdIndex].stdErrRedirectFileStr);
            debug_logger("Std Out Redirect: %d, File: %s",
                        cmdInfo->cmdMatrix[cmdIndex].isStdOutRedirect,
                        cmdInfo->cmdMatrix[cmdIndex].stdOutRedirectFileStr);
            debug_logger("Std In Redirect: %d, File: %s",
                        cmdInfo->cmdMatrix[cmdIndex].isStdInRedirect,
                        cmdInfo->cmdMatrix[cmdIndex].stdInRedirectFileStr);
            debug_logger("Background: %d",cmdInfo->isBackground);
        }
    }
}
/*
 * @Function:   parse_line_to_cmd
 * @brief:      Converts an input string into a shell command
 *              Uses re-entrant strtok_r to split by pipe first, then spaces second
 */
void parse_line_to_cmd(sCommandDetail *cmdInfo, char *line, size_t charNum) {
    char *lineCpy = (char *) malloc(charNum * sizeof(char));
    char *tokenPipe = NULL;
    char *pipeStr = NULL;
    int cmdIndex = 0;

    memcpy(lineCpy, line, charNum);
    lineCpy[charNum - 1] = 0;
    
    memcpy(cmdInfo->jobInfo.inputString, lineCpy, sizeof(char) * charNum);
    
    if(lineCpy[charNum - 2] == '&') {
      	cmdInfo->isBackground = true;
      	lineCpy[charNum - 2] = 0;
    }
    memcpy(cmdInfo->jobInfo.activeString, lineCpy, sizeof(char) * charNum);

	debug_logger("%s",lineCpy);
    tokenPipe = strtok_r(lineCpy,"|", &pipeStr);

    while(NULL != tokenPipe) {
    	int *argvSize = &(cmdInfo->cmdMatrix[cmdIndex].argvSize);
    	*argvSize = 1;
        char *cmdStr = NULL;
        char *tokenCmd = strtok_r(tokenPipe," ",&cmdStr);

        while(NULL != tokenCmd) {
            if(0 == strcmp(tokenCmd, "2>")) {
                strcpy(cmdInfo->cmdMatrix[cmdIndex].stdErrRedirectFileStr, strtok_r(NULL, " ",&cmdStr));
                cmdInfo->cmdMatrix[cmdIndex].isStdErrRedirect = true;
            } else if(0 == strcmp(tokenCmd, ">")) {
                strcpy(cmdInfo->cmdMatrix[cmdIndex].stdOutRedirectFileStr, strtok_r(NULL, " ",&cmdStr));
                cmdInfo->cmdMatrix[cmdIndex].isStdOutRedirect = true;
            } else if(0 == strcmp(tokenCmd, "<")) {
                strcpy(cmdInfo->cmdMatrix[cmdIndex].stdInRedirectFileStr, strtok_r(NULL, " ",&cmdStr));
                cmdInfo->cmdMatrix[cmdIndex].isStdInRedirect = true;
            } else {	
                strncpy(cmdInfo->cmdMatrix[cmdIndex].argv[(*argvSize) - 1], tokenCmd, MAX_TOKEN_SIZE);
                (*argvSize)++;
            }
            tokenCmd = strtok_r(NULL, " ",&cmdStr);
        }
        tokenPipe = strtok_r(NULL,"|",&pipeStr);
        cmdIndex++;
    }
    free(lineCpy);
}
/*
 * @Function:   conf_redirection
 * @brief:      Configures redirection for each command(between pipes)
 *              If input redirection fails, return a yash error
 */
static void conf_redirection(sCommandDetail *cmdInfo, int cmdIndex) {
    debug_logger("[%s]: Here",__func__);
    if(cmdInfo->cmdMatrix[cmdIndex].isStdInRedirect) {
        if(-1 == (access(cmdInfo->cmdMatrix[cmdIndex].stdInRedirectFileStr, R_OK))) {
            printf("yash: %s: No such file or directory\n",cmdInfo->cmdMatrix[cmdIndex].stdInRedirectFileStr);
            exit(1);
        } else {
            int fd0 = open(cmdInfo->cmdMatrix[cmdIndex].stdInRedirectFileStr, O_RDONLY);
            dup2(fd0, STDIN_FILENO);
            close(fd0);
        }
    }
    if(cmdInfo->cmdMatrix[cmdIndex].isStdOutRedirect) {
        int std_out_redir_fd = open(cmdInfo->cmdMatrix[cmdIndex].stdOutRedirectFileStr, O_TRUNC|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        if((-1 != std_out_redir_fd) && (-1 != dup2(std_out_redir_fd, fileno(stdout)))) {
            fflush(stdout); 
            close(std_out_redir_fd);
        }else{
           debug_logger("No Permissions to create or read file");
        }
    }
    if(cmdInfo->cmdMatrix[cmdIndex].isStdErrRedirect) {
        int std_err_redir_fd = open(cmdInfo->cmdMatrix[cmdIndex].stdErrRedirectFileStr,  O_TRUNC|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        if((-1 != std_err_redir_fd) && (-1 != dup2(std_err_redir_fd, fileno(stderr)))) {
            fflush(stderr); close(std_err_redir_fd);
        }else{
            debug_logger("No Permissions to create or read file");       
        }
    }
}


/*
 * @Function:   print_done_jobs
 * @brief:      Used by the yash parent to print latest Done jobs in jobs format
 *              after a newline is detected in the prompt
 *              Triggers a command to remove those jobs from memory
 */
static void print_done_jobs(sYashThreadType *yash_thread_info) {
    sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
    for(int i = 0; i < MAX_JOBS; i++) {
        if(PROC_DONE == yash_thread_info->jobs[i].jobInfo.status) {
        	EProcStatus status = yash_thread_info->jobs[i].jobInfo.status;
            dprintf(yash_thread_info->sock_fd,
            		"[%d]%s%-10s%30s &\n",
            		i,"-", PROC_STRING[status],
            		yash_thread_info->jobs[i].jobInfo.activeString);
            sem_post(job_sem);
            remove_job_cmd(yash_thread_info, yash_thread_info->jobs[i].jobInfo.pgId);
            sem_wait(job_sem);
        }
    }
    sem_post(job_sem);
}
/*
 * @Function:   jobs
 * @brief:      Implementation of bash-like jobs command,
 *              pipe-able and handles redirection since it's forked
 *              from yash
 */
static void jobs(sYashThreadType *yash_thread_info) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	for(int i = 0; i < MAX_JOBS; i++) {
        if(yash_thread_info->jobs[i].jobInfo.pgId) {
            EProcStatus status = yash_thread_info->jobs[i].jobInfo.status;
            char plusMinus, bgFg;
            if(i == (yash_thread_info->jobsNextIndex - 1)) {
            	plusMinus = '+';
            } else {
            	plusMinus = '-';
            }
            if(PROC_RUN == (yash_thread_info->jobs[i].jobInfo.status)) {
            	bgFg = '&';
            } else {
            	bgFg = ' ';
            }
            dprintf(yash_thread_info->sock_fd,
            		"[%d]%c%-10s%30s %c\n",
            		i,plusMinus,PROC_STRING[status],
            		yash_thread_info->jobs[i].jobInfo.activeString,
            		bgFg);
        }
    }
	sem_post(job_sem); 
}

/*
 * @Function:   run_cmd_regular
 * @brief:      Execution of single command, used by pipe API
 *              Jobs implementation implemented in this file
 *              fg and bg will return immediately due to special handling
 *              in yash
 *              Most commands will be searched on the path
 */
static void run_cmd_regular(sCommandDetail *cmdInfo, int cmdIndex) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    conf_redirection(cmdInfo, cmdIndex);
    for(int i = 0; i < cmdInfo->cmdMatrix[cmdIndex].argvSize - 1; i++) {
    	cmdInfo->cmdMatrix[cmdIndex].argvExec[i] = cmdInfo->cmdMatrix[cmdIndex].argv[i];
    }
    if(0 > execvp(cmdInfo->cmdMatrix[cmdIndex].argv[0], cmdInfo->cmdMatrix[cmdIndex].argvExec)) {
        exit(1);
    }
}
/*
 * @Function:   run_cmd_pipe
 * @brief:      Execution split to handle piping, creates a child to do the first command in the pipe
 *              and allows the parent to be the process waiting on that child command.
 */
static void run_cmd_pipe(sCommandDetail *cmdInfo) {
    int fd[2];
    pipe(fd);
    pid_t childpid = fork();
    if(0 == childpid) {
        close(fd[0]);
        dup2(fd[1], 1);
        run_cmd_regular(cmdInfo, 0);
    } else {
        close(fd[1]);
        dup2(fd[0], 0);
        run_cmd_regular(cmdInfo, 1);
    }
}

static void fg(sYashThreadType *yash_thread_info) {
	sCommandDetail cmdInfo = {0};
	get_last_job_cmd(yash_thread_info, &cmdInfo);
	if(cmdInfo.jobInfo.pgId) {
		int status = 0;
		remove_job_cmd(yash_thread_info, cmdInfo.jobInfo.pgId);
		cmdInfo.jobInfo.status = PROC_RUN;
		push_fg_cmd(yash_thread_info, &cmdInfo);
		kill(cmdInfo.jobInfo.pgId, SIGCONT);
		
		printf("%s",cmdInfo.jobInfo.activeString);
		fflush(stdout);
		
		waitpid(cmdInfo.jobInfo.pgId, &status, WUNTRACED);
		pop_fg_cmd(yash_thread_info, &cmdInfo);
        if(WIFSTOPPED(status)) {
        	cmdInfo.jobInfo.status = PROC_STOP;
        	push_job_cmd(yash_thread_info, &cmdInfo);
        } else {
        	//command stopped
        }
	}
}
static void bg(sYashThreadType *yash_thread_info) {
	sCommandDetail cmdInfo = {0};
	get_last_stop_cmd(yash_thread_info, &cmdInfo);
	if(cmdInfo.jobInfo.pgId) {
		update_job_cmd(yash_thread_info, cmdInfo.jobInfo.pgId, PROC_RUN);
		kill(cmdInfo.jobInfo.pgId, SIGCONT);
		printf("%s &",cmdInfo.jobInfo.activeString);
		fflush(stdout);
	}
}
static void fork_exec(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	int terminalfd; // We are going to read & write to our child through it

   	//pid_t pid = forkpty(&terminalfd, NULL, NULL, NULL); 
	pid_t pid = fork();
    if(0 == pid) {
      	setpgrp();
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        if((1 < cmdInfo->cmdMatrix[0].argvSize)
        && (1 < cmdInfo->cmdMatrix[1].argvSize)) {
          	run_cmd_pipe(cmdInfo);
        } else if (1 < cmdInfo->cmdMatrix[0].argvSize) {
            run_cmd_regular(cmdInfo, 0);
        }
    } else {
    	setpgid(pid, pid);
        cmdInfo->jobInfo.pgId = pid;
        cmdInfo->jobInfo.status = PROC_RUN;
        if(!(cmdInfo->isBackground)) {
          	int status = 0;
            push_fg_cmd(yash_thread_info, cmdInfo);
            waitpid(pid, &status, WUNTRACED);
            pop_fg_cmd(yash_thread_info, cmdInfo);
            if(WIFSTOPPED(status)) {
            	cmdInfo->jobInfo.status = PROC_STOP;
            	push_job_cmd(yash_thread_info, cmdInfo);
            } else {
            	//command stopped
            }
        } else {
        	push_job_cmd(yash_thread_info, cmdInfo);
        }
    }
}
/*
 * @Function:   run_cmd
 * @brief:      Takes a parsed command, and uses a fork to have a child execute it
 *              Pipes will be treated a bit differently inside this function
 *              fg,bg,and jobs are not treated as normal commands
 *              fg and bg will execute in the parent thread
 *              jobs is supported by redirection and piping, fg and bg are not
 */
void run_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	print_done_jobs(yash_thread_info);
    if(1 < cmdInfo->cmdMatrix[0].argvSize) {
    	debug_logger("Serving Client at %s:%d",inet_ntoa(yash_thread_info->client.sin_addr), yash_thread_info->client.sin_port);
    	yashd_logger("[%s:%d]: %s",inet_ntoa(yash_thread_info->client.sin_addr),yash_thread_info->client.sin_port, cmdInfo->jobInfo.inputString);
    	if(!strcmp(cmdInfo->cmdMatrix[0].argv[0], "jobs")) {
    		jobs(yash_thread_info);
    	} else if(!strcmp(cmdInfo->cmdMatrix[0].argv[0], "bg")) {
    		bg(yash_thread_info);
    	} else if(!strcmp(cmdInfo->cmdMatrix[0].argv[0], "fg")) {
    		fg(yash_thread_info);
    	} else {
    		fork_exec(yash_thread_info, cmdInfo);
    	}
    }
}
