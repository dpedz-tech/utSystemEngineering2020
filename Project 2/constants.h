
#define NR_JOBS 20
#define PATH_BUFSIZE 2000
#define COMMAND_BUFSIZE 2000
#define TOKEN_BUFSIZE 64
#define TOKEN_DELIMITERS " \t\r\n\a"
#define PROMPT "# "
#define FILEPERMISSIONS S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH

#define BACKGROUND_EXECUTION 0
#define FOREGROUND_EXECUTION 1
#define PIPELINE_EXECUTION 2

#define COMMAND_EXTERNAL 0
#define COMMAND_JOBS 1
#define COMMAND_FG 2
#define COMMAND_BG 3

#define PRINT_OPTIONS_COMPLETED_JOBS 0
#define PRINT_OPTIONS_COMMAND_JOBS 1
#define PRINT_OPTIONS_COMMAND_FG 2
#define PRINT_OPTIONS_COMMAND_BG 3

#define STATUS_RUNNING 0
#define STATUS_DONE 1
#define STATUS_SUSPENDED 2
#define STATUS_CONTINUED 0
#define STATUS_TERMINATED 1

#define PROC_FILTER_ALL 0
#define PROC_FILTER_DONE 1
#define PROC_FILTER_REMAINING 2

#define ALLOCATION_ERROR "allocation error\n"
#define FILEDIRECTORY_ERROR "no such file or directory:\n"
#define COMMAND_ERROR "command error\n"
#define FG_COMMAND_ERROR "usage: fg \n"
#define BG_COMMAND_ERROR "usage: bg \n"
#define JOBEXISIT_ERROR "no such job\n"
#define NOJOBS_ERRORS "no jobs\n"

#define MAXHOSTNAME 80
#define BUFSIZE 1024

const char* STATUS_STRING[] = {
    "Running",
    "Done",
    "Stopped",
    "Continued",
    "Terminated"
};

struct process {
    char *command;
    int argc;
    char **argv;
    char *input_path;
    char *output_path;
    char *error_path;
    pid_t pid;
    int type;
    int status;
    struct process *next;
};

struct job {
    int id;
    struct process *root;
    char *command;
    pid_t pgid;
    int mode;
    int bgcommand;
};

struct shell_info {
    char cur_user[TOKEN_BUFSIZE];
    char cur_dir[PATH_BUFSIZE];
    char pw_dir[PATH_BUFSIZE];
    struct job *jobs[NR_JOBS + 1];
};

struct shell_info *shell;

int signal_recieved =0;
int rc, cc;
int   sd;
char buf[BUFSIZE];
char rbuf[BUFSIZE];
void cleanup(char *buf);