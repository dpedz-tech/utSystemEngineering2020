
// Reference: https://github.com/hungys/mysh
// Dale Pedzinski


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <glob.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "constants.h"
#include "jobManager.c"
#include "customCommands.c"

static void sig_handler(int signal) { 
	switch(signal){ 
		case SIGINT:
		    signal_recieved =1;
            printf("\n");
			break; 
		case SIGTSTP:
		    signal_recieved =1;
		    printf("\n");
			break;
	}
}

int launch_custom_commands(struct process *proc) {
    int status = 1;

    switch (proc->type) {
        case COMMAND_JOBS:
            command_jobs(proc->argc, proc->argv);
            break;
        case COMMAND_FG:
            command_fg(proc->argc, proc->argv);
            break;
        case COMMAND_BG:
            command_bg(proc->argc, proc->argv);
            break;
        default:
            status = 0;
            break;
    }

    return status;
}

int launch_command_helper(struct job *job, struct process *proc, int in_fd, int out_fd, int error_fd, int mode) {
    proc->status = STATUS_RUNNING;
    if (proc->type != COMMAND_EXTERNAL && launch_custom_commands(proc)) {
        return 0;
    }

    pid_t childpid;
    int status = 0;

    childpid = fork();

    if (childpid < 0) {
        return -1;
    } else if (childpid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        proc->pid = getpid();
        if (job->pgid > 0) {
            setpgid(0, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(0, job->pgid);
        }

        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }

        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }
        
        if (error_fd != 2) {
            dup2(error_fd, 2);
            close(error_fd);
        }

        if (execvp(proc->argv[0], proc->argv) < 0) {
            fprintf(stderr,COMMAND_ERROR);
            exit(0);
        }

        exit(0);
    } else {
        proc->pid = childpid;
        if (job->pgid > 0) {
            setpgid(childpid, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(childpid, job->pgid);
        }

        if (mode == FOREGROUND_EXECUTION) {
            tcsetpgrp(0, job->pgid);
            status = wait_for_job(job->id);
            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        }
    }

    return status;
}

int launch_commander(struct job *job) {
    struct process *proc;
    int status = 0;
    int in_fd = 0;
    int fd[3];
    int job_id = -1;

    check_jobs();
    if (job->root->type == COMMAND_EXTERNAL) {
        job_id = insert_job(job);
    }

    for (proc = job->root; proc != NULL; proc = proc->next) {
        if (proc == job->root ) {
            if (proc->input_path != NULL){
                in_fd = open(proc->input_path, O_RDONLY);
                if (in_fd < 0) {
                    fprintf(stderr,"%s %s\n",FILEDIRECTORY_ERROR, proc->input_path);
                    remove_job(job_id);
                    return -1;
                }
            }
        }
        if (proc->next != NULL) {
            pipe(fd);
            status = launch_command_helper(job, proc, in_fd, fd[1], 2, PIPELINE_EXECUTION);
            close(fd[1]);
            in_fd = fd[0];
        } else {
            int out_fd = 1;
            if (proc->output_path != NULL) {
                out_fd = open(proc->output_path, O_CREAT|O_WRONLY, FILEPERMISSIONS);
                if (out_fd < 0) {
                    out_fd = 1;
                }
            }
            int error_fd = 2;
            if (proc->error_path != NULL) {
                error_fd = open(proc->error_path, O_CREAT|O_WRONLY, FILEPERMISSIONS);
                if (error_fd < 0) {
                    error_fd = 2;
                    }
            }
            status = launch_command_helper(job, proc, in_fd, out_fd, error_fd, job->mode);
        }
    }

    if (job->root->type == COMMAND_EXTERNAL) {
        if (status >= 0 && job->mode == FOREGROUND_EXECUTION) {
            remove_job(job_id);
        } else if (job->mode == BACKGROUND_EXECUTION) {
            print_processes_of_job(job_id);
        }
    }

    return status;
}

int get_process_type(char *command) {
    if (strcmp(command, "jobs") == 0) {
        return COMMAND_JOBS;
    } else if (strcmp(command, "fg") == 0) {
        return COMMAND_FG;
    } else if (strcmp(command, "bg") == 0) {
        return COMMAND_BG;
    } else {
        return COMMAND_EXTERNAL;
    }
}

struct process* parse_segment_commander(char *segment) {
    int bufsize = TOKEN_BUFSIZE;
    int position = 0;
    char *command = strdup(segment);
    char *token;
    char **tokens = (char**) malloc(bufsize * sizeof(char*));

    if (!tokens) {
        fprintf(stderr, ALLOCATION_ERROR);
        exit(EXIT_FAILURE);
    }

    token = strtok(segment, TOKEN_DELIMITERS);
    while (token != NULL) {
        glob_t glob_buffer;
        int glob_count = 0;
        if (strchr(token, '*') != NULL || strchr(token, '?') != NULL) {
            glob(token, 0, NULL, &glob_buffer);
            glob_count = glob_buffer.gl_pathc;
        }

        if (position + glob_count >= bufsize) {
            bufsize += TOKEN_BUFSIZE;
            bufsize += glob_count;
            tokens = (char**) realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, ALLOCATION_ERROR);
                exit(EXIT_FAILURE);
            }
        }

        if (glob_count > 0) {
            int i;
            for (i = 0; i < glob_count; i++) {
                tokens[position++] = strdup(glob_buffer.gl_pathv[i]);
            }
            globfree(&glob_buffer);
        } else {
            tokens[position] = token;
            position++;
        }

        token = strtok(NULL, TOKEN_DELIMITERS);
        
    }

    int i = 0, argc = 0;
    char *input_path = NULL;
    char *output_path = NULL;
    char *error_path = NULL;
    while (i < position) {
        if (strcmp(tokens[i],">") == 0|| strcmp(tokens[i],"<") == 0 || strcmp(tokens[i],"2>") == 0) {
            break;
        }
        i++;
    }
    argc = i;

    for (; i < position; i++) {
        if (strcmp(tokens[i],"<") == 0) {
            if (strlen(tokens[i]) == 1) {
                input_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(input_path, tokens[i + 1]);
                i++;
            } else {
                input_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(input_path, tokens[i] + 1);
            }
        } else if (strcmp(tokens[i],">") == 0) {
            if (strlen(tokens[i]) == 1) {
                output_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(output_path, tokens[i + 1]);
                i++;
            } else {
                output_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(output_path, tokens[i] + 1);
            }
        } else if (strcmp(tokens[i],"2>") == 0) {
            if (strlen(tokens[i]) == 2) {
                error_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(error_path, tokens[i + 1]);
                i++;
            } else {
                error_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(error_path, tokens[i] + 1);
            }
            
        }
        else {
            break;
        }
    }

    for (i = argc; i <= position; i++) {
        tokens[i] = NULL;
    }

    struct process *new_proc = (struct process*) malloc(sizeof(struct process));
    new_proc->command = command;
    new_proc->argv = tokens;
    new_proc->argc = argc;
    new_proc->input_path = input_path;
    new_proc->output_path = output_path;
    new_proc->error_path = error_path;
    new_proc->pid = -1;
    new_proc->type = get_process_type(tokens[0]);
    new_proc->next = NULL;
    return new_proc;
}

char* helper_strtrim(char* line) {
    char *head = line, *tail = line + strlen(line);

    while (*head == ' ') {
        head++;
    }
    while (*tail == ' ') {
        tail--;
    }
    *(tail + 1) = '\0';

    return head;
}

struct job* parse_commander(char *line) {
    line = helper_strtrim(line);
    char *command = strdup(line);

    struct process *root_proc = NULL;
    struct process *proc = NULL;
    char *line_cursor = line;
    char *c = line;
    char *seg;
    int seg_len = 0;
    int mode = FOREGROUND_EXECUTION;
    int bgcommand = -1;

    if (line[strlen(line) - 1] == '&') {
        mode = BACKGROUND_EXECUTION;
        bgcommand = BACKGROUND_EXECUTION;
        line[strlen(line) - 1] = '\0';
    }

    while (1) {
        if (*c == '\0' || *c == '|') {
            seg = (char*) malloc((seg_len + 1) * sizeof(char));
            strncpy(seg, line_cursor, seg_len);
            seg[seg_len] = '\0';

            struct process* new_proc = parse_segment_commander(seg);
            if (!root_proc) {
                root_proc = new_proc;
                proc = root_proc;
            } else {
                proc->next = new_proc;
                proc = new_proc;
            }

            if (*c != '\0') {
                line_cursor = c;
                while (*(++line_cursor) == ' ');
                c = line_cursor;
                seg_len = 0;
                continue;
            } else {
                break;
            }
        } else {
            seg_len++;
            c++;
        }
    }

    struct job *new_job = (struct job*) malloc(sizeof(struct job));
    new_job->root = root_proc;
    new_job->command = command;
    new_job->pgid = -1;
    new_job->mode = mode;
    new_job->bgcommand = bgcommand;
    return new_job;
}

char* read_line() {
    int bufsize = COMMAND_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, ALLOCATION_ERROR);
        exit(EXIT_FAILURE);
    }

    while (1) {
        c = getchar();

        if( c == '\n' || c == '\0' || (c == EOF && signal_recieved ==1))  {
            buffer[position] = '\0';
            signal_recieved=0;
            return buffer;
        }
        else if (c == EOF && signal_recieved ==0 ){
            return "EXIT";
        }
        else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize) {
            bufsize += COMMAND_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, ALLOCATION_ERROR);
                exit(EXIT_FAILURE);
            }
        }
    }
}

void process_loop() {
    char *line;
    struct job *job;
    int status = 1;

    while (1) {
        printf(PROMPT);
        line = read_line();
        if (line == "EXIT"){
            break;
        }
        if (line == "\0" || line == NULL || line == "0" ){
            continue;
        }
        if (strlen(line) == 0) {
            check_jobs();
            continue;
        }
        
        job = parse_commander(line);
        status = launch_commander(job);
    }
}

void init() {
    struct sigaction sigint_action = {
        .sa_handler = &sig_handler,
        .sa_flags = 0
    };
    struct sigaction sigstp_action = {
        .sa_handler = &sig_handler,
        .sa_flags = 0
    };
    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);
    sigemptyset(&sigstp_action.sa_mask);
    sigaction(SIGTSTP, &sigstp_action, NULL);
    pid_t pid = getpid();
    setpgid(pid, pid);
    tcsetpgrp(0, pid);

    shell = (struct shell_info*) malloc(sizeof(struct shell_info));
    getlogin_r(shell->cur_user, sizeof(shell->cur_user));

    struct passwd *pw = getpwuid(getuid());
    strcpy(shell->pw_dir, pw->pw_dir);

    int i;
    for (i = 0; i < NR_JOBS; i++) {
        shell->jobs[i] = NULL;
    }
}

int main(int argc, char **argv) {
    init();
    process_loop();

    return EXIT_SUCCESS;
}