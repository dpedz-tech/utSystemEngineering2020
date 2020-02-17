
int command_jobs(int argc, char **argv) {
    int i;
    for (i = 0; i < NR_JOBS; i++) {
        int bufsize = 2*COMMAND_BUFSIZE;
        char *fullCommand = malloc(sizeof(char) * bufsize);
        char *status;
        if (shell->jobs[i] != NULL) {
            struct process *proc;
            for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
                if (proc->next != NULL) {
                    strcat(fullCommand," ");
                    strcat(fullCommand,proc->command);
                    strcat(fullCommand," | ");
                }else {
                    strcat(fullCommand,proc->command);
                    status=(char *)STATUS_STRING[proc->status];
                }
            }
            if(shell->jobs[i]->mode == BACKGROUND_EXECUTION){
                    strcat(fullCommand," &");
            }
            int y = i+1;
            if (shell->jobs[y]!= NULL){
                printf("[%d]-%s\t%s",i,status, fullCommand);
                if(status == "Done"){
                    remove_job(i);
                }
            }else{
                printf("[%d]+%s\t%s", i,status, fullCommand);
                if(status == "Done"){
                    remove_job(i);
                }
            }
            printf("\n");
        }
    }
    return 0;
}

int command_fg(int argc, char **argv) {
    if (argc !=1) {
        fprintf(stderr,FG_COMMAND_ERROR);
        return -1;
    }

    int status;
    pid_t pid;
    int job_id = -1;
    
    job_id= get_last_job_id();
    pid = get_pgid_by_job_id(job_id);
    if (pid < 0) {
            fprintf(stderr,JOBEXISIT_ERROR);
            return -1;
    }

    if (kill(-pid, SIGCONT) < 0) {
        fprintf(stderr,JOBEXISIT_ERROR);
        return -1;
    }

    tcsetpgrp(0, pid);

    if (job_id > 0) {
        set_job_status(job_id, STATUS_CONTINUED);
        print_job_status(job_id,PRINT_OPTIONS_COMMAND_FG);
        if (wait_for_job(job_id) >= 0) {
            remove_job(job_id);
        }
    } else {
        wait_for_pid(pid);
    }

    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(0, getpid());
    signal(SIGTTOU, SIG_DFL);

    return 0;
}

int command_bg(int argc, char **argv) {
    if (argc != 1) {
        fprintf(stderr,BG_COMMAND_ERROR);
        return -1;
    }

    pid_t pid;
    int job_id = -1;

    job_id= get_last_job_id();
    pid = get_pgid_by_job_id(job_id);
    if (pid < 0) {
            fprintf(stderr,JOBEXISIT_ERROR);
            return -1;
    }

    if (kill(-pid, SIGCONT) < 0) {
        fprintf(stderr,JOBEXISIT_ERROR);
        return -1;
    }

    if (job_id > 0) {
        set_job_status(job_id, STATUS_CONTINUED);
        print_job_status(job_id, PRINT_OPTIONS_COMMAND_BG);
    }

    return 0;
}