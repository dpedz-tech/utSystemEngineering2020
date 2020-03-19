#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "yash.h"
#include "yashd.h"
#include "yash_thread.h"

#define READY_PROMPT "\n# "

extern void init_cmd_info(sCommandDetail *cmdInfo);
extern void destroy_cmd_info(sCommandDetail *cmdInfo);
extern void parse_line_to_cmd(sCommandDetail *cmdInfo, char *line, size_t charNum);
extern void print_cmd_info(sCommandDetail *cmdInfo);
extern void run_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo);

void yashd_handle_msg(sYashThreadType *yash_thread_info, char *buf);

static inline void printReady(int sock_fd) {
	int rc = write(sock_fd, READY_PROMPT, strlen(READY_PROMPT));
}
static inline void printReadyPlusCmd(int sock_fd, sCommandDetail *cmdInfo) {
	size_t needed = snprintf(NULL, 0, "%s%s", READY_PROMPT, cmdInfo->jobInfo.inputString) + 1;
    char  *buffer = malloc(needed);
    sprintf(buffer, "%s%s", READY_PROMPT, cmdInfo->jobInfo.inputString);
	int rc = write(sock_fd, buffer, strlen(buffer));
	free(buffer);
}
void yash_thread_exit(sYashThreadType *yash_thread_info) {
	int status = 0;
	debug_logger("Exit");
	close(yash_thread_info->sock_fd);
	close(yash_thread_info->cmd_pipe[0]);
	close(yash_thread_info->cmd_pipe[1]);
	close(yash_thread_info->ctl_pipe[0]);
	close(yash_thread_info->ctl_pipe[1]);

	kill(yash_thread_info->childPid, SIGTERM);
	waitpid(yash_thread_info->childPid, &status, WUNTRACED);

	free(yash_thread_info);
	pthread_exit(NULL);
}
void *cmd_loop(void *args) {
	sYashThreadType *yash_thread_info = (sYashThreadType *) args;
	debug_logger("[%s]: Entered", __func__);

	printReady(yash_thread_info->sock_fd);
	while(1) {
		sCommandDetail cmdInfo = {0};
		debug_logger("Waiting for Piped Cmd");
		//Set pipe to blocking
		int opts = fcntl(yash_thread_info->cmd_pipe[0],F_GETFL) & (~O_NONBLOCK);
		fcntl(yash_thread_info->cmd_pipe[0],F_SETFL,opts);
		read(yash_thread_info->cmd_pipe[0], &cmdInfo, sizeof(cmdInfo));
		print_cmd_info(&cmdInfo);
		run_cmd(yash_thread_info, &cmdInfo);
		//Set pipe to non-blocking
		fcntl(yash_thread_info->cmd_pipe[0], F_SETFL, O_NONBLOCK);
		memset(&cmdInfo, 0, sizeof(cmdInfo));
		while(-1 != read(yash_thread_info->cmd_pipe[0], &cmdInfo, sizeof(cmdInfo))) {
			print_cmd_info(&cmdInfo);
			printReadyPlusCmd(yash_thread_info->sock_fd, &cmdInfo);
			run_cmd(yash_thread_info, &cmdInfo);
			memset(&cmdInfo, 0, sizeof(cmdInfo));
		}
		printReady(yash_thread_info->sock_fd);
	}
	return NULL;
}
void *ctl_loop(void *args) {
	sYashThreadType *yash_thread_info = (sYashThreadType *) args;
	debug_logger("[%s]: Entered", __func__);
	while(1) {
		ECtlType ctlType;
		debug_logger("[%s]: Waiting for read", __func__);
		if(0 < read(yash_thread_info->ctl_pipe[0], &ctlType, sizeof(ctlType))) {
			sCommandDetail cmdInfo = {0};
			get_fg_cmd(yash_thread_info, &cmdInfo);
			if(cmdInfo.jobInfo.pgId) {
				switch(ctlType) {
					case CTL_INT:
						debug_logger("[%s]: Sending Int", __func__);
						kill(cmdInfo.jobInfo.pgId, SIGINT);
						break;
					case CTL_STOP:
						debug_logger("[%s]: Sending Stop", __func__);
						kill(cmdInfo.jobInfo.pgId, SIGTSTP);
						break;
					default:;
				}
			}
		}
	}
	return NULL;
}
void *reap_loop(void *args) {
	sYashThreadType *yash_thread_info = (sYashThreadType *) args;
	debug_logger("[%s]: Entered", __func__);
	while(1) {
		int status = 0;
		pid_t pid = waitpid(-1, &status, 0);
		if(0 < pid) {
			debug_logger("[%s]: PID: %d Wait Status: %d",__func__,pid, status);
			if(WIFEXITED(status)) {
				update_job_cmd(yash_thread_info, pid, PROC_DONE);
			}
		}
	}
}
static void print_yash_thd_info(sYashThreadType *yash_thread_info) {
	debug_logger("sock_fd", yash_thread_info->sock_fd);
	debug_logger("cmd_pipe %d %d", yash_thread_info->cmd_pipe[0],yash_thread_info->cmd_pipe[0] );
	debug_logger("ctl_pipe %d %d", yash_thread_info->ctl_pipe[0],yash_thread_info->ctl_pipe[0] );
}

void *yash_thread(void *args) {
    if(!args) {
    	return NULL;
    }
    sYashThreadType *yash_thread_info = (sYashThreadType *) args;

	debug_logger("Entered");
	
	if(pipe(yash_thread_info->cmd_pipe)) {
		yash_thread_exit(yash_thread_info);
	} else if(pipe(yash_thread_info->ctl_pipe)) {
		yash_thread_exit(yash_thread_info);
	}
	print_yash_thd_info(yash_thread_info);

	pid_t pid = fork();
	
	if(0 == pid) {
		print_yash_thd_info(yash_thread_info);
		sYashThreadType *ysh_thd_data = malloc(sizeof(sYashThreadType));

		memcpy(ysh_thd_data, yash_thread_info, sizeof(sYashThreadType));
		pthread_t cmd_loop_thd_id, ctl_loop_thd_id, reap_loop_thd_id = 0;

		close(ysh_thd_data->cmd_pipe[1]);
		close(ysh_thd_data->ctl_pipe[1]);

		close(fileno(stdout));
		close(fileno(stderr));
		dup2(ysh_thd_data->sock_fd, fileno(stdout));
		dup2(ysh_thd_data->sock_fd, fileno(stderr));

		sem_init(&(ysh_thd_data->job_sem), 0, 1);
		
		if(pthread_create(&cmd_loop_thd_id, NULL, &cmd_loop, (void *)ysh_thd_data)) {

		} else if(pthread_create(&ctl_loop_thd_id, NULL, &ctl_loop, (void *)ysh_thd_data)) {

		} else if(pthread_create(&reap_loop_thd_id, NULL, &reap_loop, (void *)ysh_thd_data)) {
		
		} else {
			debug_logger("Pthread Create");
			pthread_join(cmd_loop_thd_id, NULL);
			pthread_join(ctl_loop_thd_id, NULL);
		}
		free(ysh_thd_data);
		exit(1);
	} else {
		yash_thread_info->childPid = pid;
		debug_logger("Child PID: %d",yash_thread_info->childPid);
		close(yash_thread_info->cmd_pipe[0]);
		close(yash_thread_info->ctl_pipe[0]);
		while(1) {
			char buf[1024] = {0};
			debug_logger("Waiting For Response...");
			int sock_rc = recv(yash_thread_info->sock_fd,buf,sizeof(buf), 0);
			if(0 < sock_rc) {
				buf[sock_rc] = '\0';
				debug_logger("Message received: %s", buf);
				yashd_handle_msg(yash_thread_info, buf);
			} else if(0 == sock_rc) {
				yash_thread_exit(yash_thread_info);
			} else {
				debug_logger("Recv Err: %d", sock_rc);
			}
		}
	}
}
static void yashd_handle_ctl(sYashThreadType *yash_thread_info, char *buf) {
	ECtlType ctlType;

	if(0 == strncmp(buf, "d\n", 2)) {
		yash_thread_exit(yash_thread_info);
	} else if(0 == strncmp(buf, "c\n", 2)) {
		debug_logger("Kill FG Process, if any");
		ctlType = CTL_INT;
		write(yash_thread_info->ctl_pipe[1], &ctlType, sizeof(ctlType));
		debug_logger("After Write CTL");
	} else if(0 == strncmp(buf, "z\n", 2)) {
		debug_logger("Stop FG Process, if any");
		ctlType = CTL_STOP;
		write(yash_thread_info->ctl_pipe[1], &ctlType, sizeof(ctlType));
	}
}
static void yashd_handle_cmd(sYashThreadType *yash_thread_info, char *buf) {
	sCommandDetail cmdInfo = {0};
	parse_line_to_cmd(&cmdInfo, buf, strlen(buf));
	write(yash_thread_info->cmd_pipe[1], &cmdInfo, sizeof(cmdInfo));
}
void yashd_handle_msg(sYashThreadType *yash_thread_info, char *buf) {
	if(0 == strncmp(buf, "CMD ", 4)) {
		debug_logger("CMD detected");
		yashd_handle_cmd(yash_thread_info, buf + 4);
	} else if(0 == strncmp(buf, "CTL ", 4)) {
		debug_logger("CTL detected");
		yashd_handle_ctl(yash_thread_info, buf + 4);
	} else {
		debug_logger("Invalid Input Detected");
	}
}
