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
#include "yash.h"
#include "yashd.h"
#include "yash_thread.h"

//static sJobType jobsTable[MAX_JOBS_SUPPORTED] = {0};
//static int jobsNextIndex = 0;

/*
 * @Function:   find_job
 * @brief:      Find the job with the given PG ID
 *              Returns NULL if not found
 */
/*static sJobType * find_job(pid_t pgId)
{
    for(int i = 0; i < MAX_JOBS_SUPPORTED; i++)
    {
        if(jobsTable[i].pgId == pgId)
        {
            return (&(jobsTable[i]));
        }
    }
    return NULL;
}*/
/*
 * @Function:   find_last_stopped_job
 * @brief:      Returns the information for the last stopped job
 *              Used by BG command
 *              Returns NULL if not found
 */
/*static sJobType *find_last_stopped_job(void)
{
    for(int i = MAX_NON_FG_JOBS_SUPPORTED - 1; i >= 0; i--)
    {
        if(jobsTable[i].pgId && PROC_STOP == jobsTable[i].status)
        {
            return (&(jobsTable[i]));
        }
    }
    return NULL;
}*/
/*
 * @Function:   update_job
 * @brief:      Updates the running status of a job
 */
/*static void update_job(sJobType *jobEntry)
{
    sJobType *found_job = find_job(jobEntry->pgId);
    if(found_job)
    {
        found_job->status = jobEntry->status;
    }
}*/
/*
 * @Function:   push_job
 * @brief:      Pushes a job into the job data structure
 *              If a Foreground job, put at a certain index
 *              If a background job, insert into last available index
 */
/*static int push_job(sJobType *jobEntry, bool isFg)
{
    int pushedIndex = -1;
    if(isFg)
    {
        pushedIndex = FG_JOB_INDEX;
        memcpy(&(jobsTable[FG_JOB_INDEX]), jobEntry, sizeof(sJobType));
    }
    else
    {
        pushedIndex = jobsNextIndex;
        memcpy(&(jobsTable[jobsNextIndex]), jobEntry, sizeof(sJobType));
        jobsNextIndex++;
    }
    return pushedIndex;
}*/
/*
 * @Function:   remove_job
 * @brief:      Removes a job specified at index
 *              Foreground job exempt from the updating the next index counter
 */
/*static void remove_job(int jobIndex)
{
    if(FG_JOB_INDEX == jobIndex)
    {
        memset(&(jobsTable[FG_JOB_INDEX]), 0, sizeof(sJobType));
    }
    else
    {
        memset(&(jobsTable[jobIndex]), 0, sizeof(sJobType));
        if((jobsNextIndex - 1) == jobIndex)
        {
            while(jobsNextIndex && !(jobsTable[jobsNextIndex - 1].pgId))
            {
               jobsNextIndex--;
            } 
        }
    }
}*/

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
            } else if(0 == strcmp(tokenCmd, "&")) {
                cmdInfo->isBackground = true;
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
        int std_out_redir_fd = open(cmdInfo->cmdMatrix[cmdIndex].stdOutRedirectFileStr, O_TRUNC | O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        if((-1 != std_out_redir_fd) && (-1 != dup2(std_out_redir_fd, fileno(stdout)))) {
            fflush(stdout); close(std_out_redir_fd);
        }
    }
    if(cmdInfo->cmdMatrix[cmdIndex].isStdErrRedirect) {
        int std_err_redir_fd = open(cmdInfo->cmdMatrix[cmdIndex].stdErrRedirectFileStr, O_TRUNC | O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        if((-1 != std_err_redir_fd) && (-1 != dup2(std_err_redir_fd, fileno(stderr)))) {
            fflush(stderr); close(std_err_redir_fd);
        }
    }
}


/*
 * @Function:   print_done_jobs
 * @brief:      Used by the yash parent to print latest Done jobs in jobs format
 *              after a newline is detected in the prompt
 *              Triggers a command to remove those jobs from memory
 */
/*static void print_done_jobs(void)
{
    for(int i = 0; i < MAX_NON_FG_JOBS_SUPPORTED; i++)
    {
        if(jobsTable[i].pgId && PROC_DONE == jobsTable[i].status)
        {
            printf("[%d]%s %-20s%-20s\n",
                i,
                "-",
                PROC_STRING[jobsTable[i].status],
                jobsTable[i].inputString);
            remove_job(i);
        }
    }
}*/
/*
 * @Function:   jobs
 * @brief:      Implementation of bash-like jobs command,
 *              pipe-able and handles redirection since it's forked
 *              from yash
 */
/*static void jobs(void)
{
    for(int i = 0; i < MAX_NON_FG_JOBS_SUPPORTED; i++)
    {
        if(jobsTable[i].pgId)
        {
            printf("[%d]%s %-20s%-20s\n",
                i,
                (i == jobsNextIndex - 1)? "+":"-",
                PROC_STRING[jobsTable[i].status],
                jobsTable[i].inputString);
        }
    }
    exit(0);
}*/
/*
 * @Function:   run_cmd_regular
 * @brief:      Execution of single command, used by pipe API
 *              Jobs implementation implemented in this file
 *              fg and bg will return immediately due to special handling
 *              in yash
 *              Most commands will be searched on the path
 */
static void run_cmd_regular(sCommandDetail *cmdInfo, int cmdIndex) {
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
void get_fg_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	memcpy(cmdInfo, &(yash_thread_info->fgInfo), sizeof(sCommandDetail));
	sem_post(job_sem);
}
static void push_fg_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	memcpy(&(yash_thread_info->fgInfo), cmdInfo, sizeof(sCommandDetail));
	sem_post(job_sem);
}
static void pop_fg_cmd(sYashThreadType *yash_thread_info, sCommandDetail *cmdInfo) {
	sem_t *job_sem = &(yash_thread_info->job_sem);
	sem_wait(job_sem);
	debug_logger("[%s]: Here",__func__);
	memset(&(yash_thread_info->fgInfo), 0, sizeof(sCommandDetail));
	sem_post(job_sem);
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
    if(1 < cmdInfo->cmdMatrix[0].argvSize) {
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
            if(!(cmdInfo->isBackground)) {
              	int status = 0;
              	push_fg_cmd(yash_thread_info, cmdInfo);
                waitpid(pid, &status, WUNTRACED);
                pop_fg_cmd(yash_thread_info, cmdInfo);
                if(SIGTSTP == status) {
                	//push_bg_cmd(&cmdInfo);
                } else {
                }
            }
        }
    }
}
/*
 * @Function:   reclaim_tc
 * @brief:      Helper function that will give terminal back to yash
 */
/*static void reclaim_tc(siginfo_t *siginfo)
{
    if((siginfo->si_pid == tcgetpgrp(STDIN_FILENO))
    || (getpgid(siginfo->si_pid) == tcgetpgrp(STDIN_FILENO)))
    {
        if (-1 == tcsetpgrp(STDIN_FILENO, getpid()))
        {
            perror("tcsetpgrp failed");
        }
    }
}*/
/*
 * @Function:   sig_action_hdlr
 * @brief:      Responds to Children Interrupts, takes actions to manage them
 */
/*static void sig_action_hdlr (int sig, siginfo_t *siginfo, void *context)
{
    int status = 0;

    if(siginfo->si_status == 0 || siginfo->si_status == SIGHUP)
    {
        sJobType jobEntry = {.pgId = siginfo->si_pid, .status = PROC_DONE};
        if(jobsTable[FG_JOB_INDEX].pgId == siginfo->si_pid)
        {
            reclaim_tc(siginfo);
            remove_job(FG_JOB_INDEX);
        }
        else
        {
            update_job(&(jobEntry));
        }
    }
    else if(siginfo->si_status == SIGINT)
    {
        reclaim_tc(siginfo);
        remove_job(FG_JOB_INDEX);
    }
    else if(siginfo->si_status == SIGTSTP)
    {
        reclaim_tc(siginfo);
        jobsTable[FG_JOB_INDEX].status = PROC_STOP;
        push_job(&(jobsTable[FG_JOB_INDEX]), false);
        remove_job(FG_JOB_INDEX);
    }
    waitpid(-1, &status, WNOHANG);
}*/

/*
 * @Function:   yash_sig_init
 * @brief:      Configures signal settings for the yash shell
 *              Children inherit these settings
 */
/*static int yash_sig_init()
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    struct sigaction act;
 
    memset (&act, '\0', sizeof(act));
 
	act.sa_sigaction = &(sig_action_hdlr);
	act.sa_flags = SA_SIGINFO | SA_NOCLDWAIT | SA_RESTART;
 
	if (0 > sigaction(SIGCHLD, &act, NULL))
	{
		perror ("sigaction");
		return 1;
	}
}*/
/*
 * @Function:   main
 * @brief:      Executes the yash shell, and stays within that execution until interrupted
 */
/*int main(int argc, char *argv[])
{
    EYashRC e_yash_rc = YASH_STATUS_OK;

	if(0 != yash_sig_init())
	{
	    return 1;
	}
    while(1)
    {
        char *line = NULL;
        size_t lineSize = 0;
        size_t charNum = 0;
 
        printf("# ");
        if(-1 !=(charNum = getline(&line,&lineSize,stdin)))
        {
            sCommandDetail cmdInfo = {0};
            //0. Allocate Command Buffer
            init_cmd_info(&cmdInfo);
            //1. Parse Input into Command Buffer
            parse_line_to_cmd(&cmdInfo, line, charNum);
            //2. Print Done Jobs(if any)
            print_done_jobs();
            //3. Execute Command
            run_cmd(&cmdInfo);
            //4. Free Command Buffer
            destroy_cmd_info(&cmdInfo);
            free(line);
        }
        else
        {
            return YASH_STATUS_EXIT_GRACEFULLY;
        }
    }
    
    return (int) e_yash_rc;
}*/
