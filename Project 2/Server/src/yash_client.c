#include <stdio.h>
/* socket(), bind(), recv, send */
#include <sys/types.h>
#include <sys/socket.h> /* sockaddr_in */
#include <netinet/in.h> /* inet_addr() */
#include <arpa/inet.h>
#include <netdb.h> /* struct hostent */
#include <string.h> /* memset() */
#include <unistd.h> /* close() */
#include <stdlib.h> /* exit() */
#include <signal.h>
#define BUFSIZE 512
#define CTLD "CTL d\n"
#define CTLC "CTL c\n"
#define CTLZ "CTL z\n"

static int sd = -1;
static void cleanup(char *buf)
{
    for(int i=0; i<BUFSIZE; i++) buf[i]='\0';
}
void ctlHandler(int sig) {
	switch(sig) {
		case SIGINT:
			send(sd, CTLC,strlen(CTLC),0);
			break;
		case SIGTSTP:
			send(sd, CTLZ,strlen(CTLZ),0);
			break;
		default:;
	}
}
int main(int argc, char **argv ) {
    struct  sockaddr_in server;
    struct hostent *hp;
    
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    sd = socket (AF_INET,SOCK_STREAM,0);
    
    server.sin_family = AF_INET;
    hp = gethostbyname(argv[1]);
    bcopy ( hp->h_addr, &(server.sin_addr.s_addr), hp->h_length);
    server.sin_port = htons(atoi(argv[2]));
    connect(sd, (struct  sockaddr *) &server, sizeof(server));

	pid_t pid = fork();
	if(pid < 0) {
		exit(0);
	}
    else if(pid) {
    	for(;;) {
    		signal(SIGINT, ctlHandler);
    		signal(SIGTSTP, ctlHandler);
    		char *line = NULL;
    		char linePayload[120] = {0};
        	size_t lineSize = 0;
        	size_t charNum = 0;
 
       		if(-1 !=(charNum = getline(&line,&lineSize,stdin))) {
       			snprintf(linePayload, 120, "CMD %s",line);
       			send(sd, linePayload, 120, 0);
       		} else {
       			send(sd, CTLD,strlen(CTLD),0);
       			kill(pid, SIGKILL);
       			exit(0);
       		}
    	}
    } else {
    	for (;;) {
    		char buf[BUFSIZE] = {0};
    		int rc = 0;
    		cleanup(buf);
			rc=read(sd,buf,sizeof(buf));
			if (rc != 0) {
				printf("%s", buf);
				fflush(stdout);
			} else {
				printf("Breaking\n");
				break;
			}
    	}
    }
}
