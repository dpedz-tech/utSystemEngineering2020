/*
 * File Name: yashd.c
 * Author: Alex Hoganson & Dale Pedzinski
 * Description: File contains main method for yash executable
 */
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include "yashd.h"
#include "yash_thread.h"

struct hostent *gethostbyname(const char *name);
void yashd_init(void);
void socket_init(void);
void reusePort(int sock);

#define PORT_NUM 3826

typedef struct {
    int yashdLog;//Lock this fd with mutex, during writes
    int debugLog;
    int errLog;
}sYashdInfo;

static int wc_sock = -1;//Welcome Socket
static sYashdInfo yashdInfo = {0};

//sem_t mutex;

/*
 * @Function:   main
 * @brief:      Inits yashd, and executes continuosly
 */
int main(int argc, char *argv[]) {
    yashd_init();
    debug_logger("[%s]: After Daemon Init",__func__);
    socket_init();
    debug_logger("[%s]: After Socket Init",__func__);
    while(1) {
        struct sockaddr_in client = {0};
        int client_size = sizeof(client);
        int client_sock = -1;
        debug_logger("Waiting for Client...\n");
        if(0 > (client_sock = accept(wc_sock, (struct sockaddr *)&client, &client_size))) {
            fprintf(stderr,"Invalid Client\n");
        } else {
        	pthread_t threadId = 0;
        	sYashThreadType *yash_thread_info = NULL;
        	debug_logger("Serving Client at %s %d",inet_ntoa(client.sin_addr),client.sin_port);
        	if(!(yash_thread_info = malloc(sizeof(sYashThreadType)))) {
        		debug_logger("Could not Alloc Mem");
        	} else {
        		yash_thread_info->sock_fd = client_sock;
        		memcpy(&(yash_thread_info->client), &client, sizeof(client));
        		if(pthread_create(&threadId, NULL, &yash_thread, (void *)yash_thread_info)) {
        			debug_logger("Failed to spawn Helper");
        		} else {
        			debug_logger("Spawned Helper with threadID: %lu",threadId);
        		}
        	}
        }
    }
}

void yashd_init(void) {
    pid_t pid = 0;
    char buff[256] = {0};
    int null_fd = -1;
    int k = 0;

    // put server in background (with init as parent)
    if(0 > (pid = fork())) {
        perror("Here");
        fprintf(stderr, "pid: %d", pid);
        exit(0);
    } else if (0 < pid) {
        exit(0);
    } else {
        // Close all file descriptors that are open
        for(k = getdtablesize() - 1; k > 0; k--) {
            close(k);
        }
        //Redirecting stderr to daemon error file
        if(-1 == (yashdInfo.errLog = fileno(fopen("/tmp/yashd.err", "aw")))) {
            exit(1);
        } else {
            dup2(yashdInfo.errLog, fileno(stderr));
        }
        // Redirect all std file descriptors to /dev/null
        if(0 > (null_fd = open("/dev/null", O_RDWR))) {
            perror("Open");
            exit(0);
        } else {
            dup2(null_fd, fileno(stdin));
            dup2(null_fd, fileno(stdout));
            close(null_fd);
        }
        // Creating yashd log file
        if(-1 == (yashdInfo.yashdLog = fileno(fopen("/tmp/yashd.log", "aw")))) {
            perror("Could not open yashd log");
            exit(1);
        }
        if(-1 == (yashdInfo.debugLog = fileno(fopen("/tmp/yashd.dbg", "w")))) {
            perror("Could not open yashd debug log");
            exit(1);
        }
        // Change directory to specified directory
        chdir("/");
        // Set umask to mask (usually 0) */
        umask(0);
        // Detach controlling terminal by becoming sesion leader
        setsid();
        pid = getpid();
        setpgrp();
        // Make sure only one server is running
        if (0 > (k = open("/tmp/yashd.pid", O_RDWR | O_CREAT, 0666))) {
            perror("Could not open pid file");
            exit(1);
        } else if(lockf(k, F_TLOCK, 0)) {
            perror("Could not lock file");
            exit(0);
        } else {
            sprintf(buff, "%6d", pid);
            write(k, buff, strlen(buff));
        }
    }
}
void socket_init(void) {
    struct sockaddr_in server = { .sin_port = htons(PORT_NUM),
                                  .sin_family = AF_INET,
                                  .sin_addr.s_addr = INADDR_ANY};
    struct  hostent *hp = NULL;
    char ThisHost[80] = {0};
    int server_size = 0;

    gethostname(ThisHost, 80);
    debug_logger("Server Running at Host Name %s",ThisHost);
    // Fetch INET address based on host name
    if (NULL == (hp = gethostbyname(ThisHost))) {
      fprintf(stderr, "Can't find host\n");
      exit(-1);
    }
    bcopy(hp->h_addr, &(server.sin_addr), hp->h_length);
    debug_logger("IP Address is %s", inet_ntoa(server.sin_addr));

    // Create Socket
    if(0 > (wc_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
        exit(-1);
    } else {
        reusePort(wc_sock);
    }
    // Bind Socket to INET address
    if(0 > bind(wc_sock, (struct sockaddr *) &server, sizeof(server))) {
        close(wc_sock);
        perror("Binding name to stream socket");
        exit(-1);
    }
    server_size = sizeof(server);
    if(getsockname (wc_sock, (struct sockaddr *)&server,&server_size) ) {
		perror("getting socket name");
		exit(0);
    } else {
    	debug_logger("Server Port is: %d", ntohs(server.sin_port));
   	}
    // Start listening on the socket for connections
    listen(wc_sock, 4);   
}

void reusePort(int sock) {
    int one = 1;
    if(0 > setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *) &one,sizeof(one))) {
	    fprintf(stderr, "[%s]: Error\n",__func__);
	    exit(-1);
	}
}

void debug_logger(const char* format, ...) {
	char logLine[200] = {0};

	va_list args;
    va_start(args, format);
    vsnprintf(logLine, 200, format, args);
    va_end(args);

	dprintf(yashdInfo.debugLog,"[%lu]: %s\n",pthread_self(),logLine);
}

void yashd_logger(const char* format, ...) {
	char logLine[200] = {0};
	char timeBuf[100] = {0};

	va_list args;
    va_start(args, format);
    vsnprintf(logLine, 200, format, args);
    va_end(args);

	time_t t = time(NULL);
	struct tm * p = localtime(&t);

	strftime(timeBuf, 100, "%b %d %X", p);

	dprintf(yashdInfo.yashdLog,"%s yashd%s\n",timeBuf,logLine);
}
