#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <netinet/in.h>

#define MAX_JOBS 100

#define MAX_NUM_ARGS 30
#define MAX_TOKEN_SIZE 30
#define MAX_PIPE_SIZE 2

typedef enum
{
    YASH_STATUS_OK = 0,
    YASH_STATUS_EXIT_GRACEFULLY,
    NUM_YASH_RC
}EYashRC;
typedef enum
{
    PROC_RUN = 0,
    PROC_STOP,
    PROC_DONE,
    NUM_PROC_STATUS_TYPES
}EProcStatus;

typedef enum
{
	CTL_INT = 0,
	CTL_STOP,
	NUM_CTL_TYPES
}ECtlType;

static const char *PROC_STRING[] = {
    "Running", "Stopped", "Done"
};
typedef struct
{
    bool isStdErrRedirect;
    bool isStdOutRedirect;
    bool isStdInRedirect;
    char argv[MAX_NUM_ARGS + 1][MAX_TOKEN_SIZE + 1];
    char *argvExec[MAX_NUM_ARGS + 1];
    int argvSize;
    char stdErrRedirectFileStr[MAX_TOKEN_SIZE + 1];
    char stdOutRedirectFileStr[MAX_TOKEN_SIZE + 1];
    char stdInRedirectFileStr[MAX_TOKEN_SIZE + 1];
}sSoloCommandInfo;

typedef struct
{
    char inputString[2001];
    char activeString[2001];
    pid_t pgId;
    EProcStatus status;
}sJobType;

typedef struct
{
    bool isBackground;
    sJobType jobInfo;

    sSoloCommandInfo cmdMatrix[MAX_PIPE_SIZE];
}sCommandDetail;

typedef struct {
	struct sockaddr_in client;
	int sock_fd;	
	int cmd_pipe[2];
	int ctl_pipe[2];
	sem_t job_sem;
	pid_t childPid;//Only use this in the main thread
	
	sCommandDetail fgInfo;
	sCommandDetail jobs[MAX_JOBS];
	int jobsNextIndex;
}sYashThreadType;


void *yash_thread(void *args);
