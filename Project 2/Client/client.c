
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
#include <sys/socket.h> /* sockaddr_in */
#include <netinet/in.h> /* inet_addr() */
#include <arpa/inet.h>
#include <netdb.h> /* struct hostent */
#include <string.h> /* memset() */
#include <readline/readline.h>
#include <readline/history.h>
#include "constants.h"

static void sig_handler(int signal) { 
	switch(signal){ 
		case SIGINT:
		    signal_recieved =1;
		    send(sd, "CTL c", rc, 0);
            printf("\n");
			break; 
		case SIGTSTP:
		    signal_recieved =1;
		    send(sd, "CTL z", rc, 0);
		    printf("\n");
			break;
	}
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
            return "quit";
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

    while (1) {
        printf(PROMPT);
        line = read_line();
        if (line == "quit"){
            send(sd, "CTL d", rc, 0);
            close(sd);
            kill(getppid(), 9);
            break;
        }
        if (line == "\0" || line == NULL || line == "0" ){
            continue;
        }
        if (rc == 0){
            break;
        }
        if(starts_with("CMD ",line) ==0){
            send(sd, line, rc, 0);
        }else{
            
        }
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

int starts_with(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre));
}

int main(int argc, char **argv) {
    int childpid;
    struct sockaddr_in server;
    struct sockaddr_in client;
    struct hostent *hp, *gethostbyname();
    struct sockaddr_in from;
    struct sockaddr_in addr;
    int fromlen;
    int length;
    char ThisHost[80];
    
    init();
    
    gethostname(ThisHost, MAXHOSTNAME);

    printf("----TCP/Client running at host NAME: %s\n", ThisHost);
    if  ( (hp = gethostbyname(ThisHost)) == NULL ) {
	fprintf(stderr, "Can't find host %s\n", argv[1]);
	exit(-1);
    }
    bcopy ( hp->h_addr, &(server.sin_addr), hp->h_length);
    printf("    (TCP/Client INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));
    
    /** get TCPServer-ex2 Host information, NAME and INET ADDRESS */
    
    if  ( (hp = gethostbyname(argv[1])) == NULL ) {
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	if ((hp = gethostbyaddr((char *) &addr.sin_addr.s_addr,
				sizeof(addr.sin_addr.s_addr),AF_INET)) == NULL) {
	    fprintf(stderr, "Can't find host %s\n", argv[1]);
	    exit(-1);
	}
    }
    printf("----TCP/Server running at host NAME: %s\n", hp->h_name);
    bcopy ( hp->h_addr, &(server.sin_addr), hp->h_length);
    printf("    (TCP/Server INET ADDRESS is: %s )\n", inet_ntoa(server.sin_addr));
    
    /* Construct name of socket to send to. */
    server.sin_family = AF_INET; 
    /* OR server.sin_family = hp->h_addrtype; */
    
    server.sin_port = htons(3826);
    
    /*   Create socket on which to send  and receive */
    
    sd = socket (AF_INET,SOCK_STREAM,0); 
    /* sd = socket (hp->h_addrtype,SOCK_STREAM,0); */
    
    if (sd<0) {
	perror("opening stream socket");
	exit(-1);
    }

    /** Connect to TCPServer-ex2 */
    if ( connect(sd, (struct sockaddr *) &server, sizeof(server)) < 0 ) {
	close(sd);
	perror("connecting stream socket");
	exit(0);
    }
    fromlen = sizeof(from);
    if (getpeername(sd,(struct sockaddr *)&from,&fromlen)<0){
	perror("could't get peername\n");
	exit(1);
    }
    printf("Connected to Daemon: ");
    printf("%s:%d\n", inet_ntoa(from.sin_addr),
	   ntohs(from.sin_port));
    if ((hp = gethostbyaddr((char *) &from.sin_addr.s_addr,
			    sizeof(from.sin_addr.s_addr),AF_INET)) == NULL)
	fprintf(stderr, "Can't find host %s\n", inet_ntoa(from.sin_addr));
    else
	printf("(Name is : %s)\n", hp->h_name);
    childpid = fork();
    if (childpid == 0) {
        process_loop();
    // GetUserInput();
    }
    
    /** get data from USER, send it SERVER,
      receive it from SERVER, display it back to USER  */
    
    for(;;) {
	cleanup(rbuf);
	if( (rc=recv(sd, rbuf, sizeof(buf), 0)) < 0){
	    perror("receiving stream  message");
	    exit(-1);
	}
	if (rc > 0){
	    rbuf[rc]='\0';
	    printf("Received: %s\n", rbuf);
	}else {
	    printf("Disconnected..\n");
	    close (sd);
	    exit(0);
	}
	
  }
}

void cleanup(char *buf)
{
    int i;
    for(i=0; i<BUFSIZE; i++) buf[i]='\0';
}

// void GetUserInput()
// {
//     for(;;) {
//     printf(PROMPT);
//     line = read_line();
// 	if (rc == 0) break;
// 	if (send(sd, buf, rc, 0) <0 )
// 	    perror("sending stream message");
//     }
//     printf ("EOF... exit\n");
//     close(sd);
//     kill(getppid(), 9);
//     exit (0);
// }
