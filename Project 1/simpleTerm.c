// adapted from https://github.com/jmreyes/simple-c-shell/blob/master/simple-c-shell.c

#include <readline/readline.h>
#include <readline/history.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include "util.h"
#include "processManager.h"

static int _childProcess =-10;
static int _debug =0;
struct Node *_childProcesses = NULL;
unsigned int_size = sizeof(int); 

static void sig_handler(int signo) { 
	switch(signo){ 
		case SIGINT:
			if(_childProcess < 0){
				if(_debug==1){
					printf("22\n# ");
				}else{
					printf("\n# ");
				}
			}
			if(_childProcess>0){
				if(_debug==1){
					printf("25 %d %d \n", _childProcess, getpid());
				}else{
					printf("\n");
				}
				kill(_childProcess,SIGTERM);
			}
			break; 
		case SIGTSTP:
			if(_childProcess < 0){
				if(_debug==1){
					printf("35\n# ");
				}else{
					printf("\n# ");
				}
			}
			if(_childProcess> 0){
				if(_debug==1){
					printf("38 %d %d \n", _childProcess, getpid());

				}else{
					printf("\n");
				}
			}
			kill(_childProcess,SIGTSTP);
			break;
	}
	if(_debug==1){
		printf("\nparent signal received %d\n", signo);
	}
}
static void sig_handler_child(int signo) { 
	// switch(signo){ 
	// 	case SIGINT:
	// 		exit(0);
	// 		break; 
	// 	case SIGTSTP:
	// 		if(_debug==1){
	// 			printf("Child Stopped\n");
	// 		}
	// 		break; 
	// 	case SIGCONT:
	// 		if(_debug==1){
	// 			printf("Child Contiuning\n");
	// 		}
	// 		break; 
	// }
	// while (waitpid(-1, NULL, WNOHANG) > 0) {
	// }
	// printf("\n");
	if(_debug==1){
		printf("\nchild signal received %d\n#", signo);
	}
}

void init(){
		// See if we are running interactively
        GBSH_PID = getpid();
        // The shell is interactive if STDIN is the terminal  
        GBSH_IS_INTERACTIVE = isatty(STDIN_FILENO);  

		if (GBSH_IS_INTERACTIVE) {
			// Loop until we are in the foreground
			while (tcgetpgrp(STDIN_FILENO) != (GBSH_PGID = getpgrp()))
					kill(GBSH_PID, SIGTTIN);             
	              
	              
	        // Set the signal handlers for SIGCHILD and SIGINT
			act_child.sa_handler = sig_handler_child;
			act_int.sa_handler = sig_handler;			
			act_stp.sa_handler = sig_handler;
			/**The sigaction structure is defined as something like
			
			struct sigaction {
				void (*sa_handler)(int);
				void (*sa_sigaction)(int, siginfo_t *, void *);
				sigset_t sa_mask;
				int sa_flags;
				void (*sa_restorer)(void);
				
			}*/
			
			sigaction(SIGCHLD, &act_child, 0);
			sigaction(SIGINT, &act_int, 0);
			sigaction(SIGTSTP, &act_stp, 0);
			
			// Put ourselves in our own process group
			setpgid(GBSH_PID, GBSH_PID); // we make the shell process the new process group leader
			GBSH_PGID = getpgrp();
			if (GBSH_PID != GBSH_PGID) {
					printf("Error, the shell is not process group leader");
					exit(EXIT_FAILURE);
			}
			// Grab control of the terminal
			tcsetpgrp(STDIN_FILENO, GBSH_PGID);  
			
			// Save default terminal attributes for shell
			tcgetattr(STDIN_FILENO, &GBSH_TMODES);

			// Get the current directory that will be used in different methods
			// currentDirectory = (char*) calloc(1024, sizeof(char));
        } else {
                printf("Could not make the shell interactive.\n");
                exit(EXIT_FAILURE);
        }
}

void launchSpaceCommand(char **args, int indexOfCommand, int backgroundTask){
	 _childProcess = fork();
	 if(_childProcess==0){
	 	// setsid();
		setpgid(_childProcess,_childProcess);
		// if (signal(SIGCONT, sig_handler_child) == SIG_ERR)
		// 	printf("signal(SIGCONT) error\n");
		// if (signal(SIGTSTP, sig_handler_child) == SIG_ERR)   
		// 	printf("signal(SIGTSTP) error\n");
		// signal(SIGTSTP, SIG_ERR);
		// signal(SIGINT, SIG_IGN);
		// signal(SIGCONT, SIG_IGN);
		if (backgroundTask ==1){
			int numTokens = 0;
			char * subcommand[1000];
			memset(subcommand, 0, sizeof(subcommand));
			while(args[numTokens] != NULL && numTokens<indexOfCommand){ 
				subcommand[numTokens]= args[numTokens];
				numTokens++;
			}
			execvp(*subcommand,subcommand);
		}else{
			execvp(*args,args);
		}
	 }else{
	 	push(&_childProcesses,&_childProcess, int_size);
    	if(backgroundTask==0){
    		waitpid(_childProcess,NULL,0);
		}else{
			printf("Executing command with PID: %d\n",_childProcess);
		}
    }
}

void fileRedirectCommand(char **args, int indexOfCommand, int option, int backgroundTask){
	int fileDescriptor;
	_childProcess = fork();
	char * _file = args[indexOfCommand+1];
	int numTokens = 0;
	char * subcommand[1000];
	memset(subcommand, 0, sizeof(subcommand));
	while(args[numTokens] != NULL && numTokens<indexOfCommand){ 
		subcommand[numTokens]= args[numTokens];
		numTokens++;
	}
	if(_childProcess==0){
		setsid();
		setpgid(_childProcess,_childProcess);
	 	mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH;
		if (option == 1){
			fileDescriptor = open(_file,  O_RDONLY, mode);  
			dup2(fileDescriptor, STDIN_FILENO);
			close(fileDescriptor);
		}else if (option == 2){
			fileDescriptor = creat(_file, mode);
			dup2(fileDescriptor,STDOUT_FILENO);
			close(fileDescriptor);
		}else if (option == 3){
			fileDescriptor = creat(_file, mode); 
			dup2(fileDescriptor,STDERR_FILENO);
			close(fileDescriptor);
		}
	 	execvp(*subcommand,subcommand);
	}else{
		push(&_childProcesses,&_childProcess, int_size);
		if(backgroundTask==0){
    		waitpid(_childProcess,NULL,0);
		}else{
			printf("Executing command with PID: %d\n",_childProcess);
		}
    }
}

void launchPipeCommand(char ** args, int isPipeTask_Index,int isBackgroundTask_Index,int isBackgroundTask){
	// File descriptors
	int filedes[2]; // pos. 0 output, pos. 1 input of the pipe
	int filedes2[2];
	
	int num_cmds = 0;
	
	char *command[256];
	
	pid_t pid;
	
	int err = -1;
	int end = 0;
	
	// Variables used for the different loops
	int i = 0;
	int j = 0;
	int k = 0;
	int l = 0;
	
	// First we calculate the number of commands (they are separated
	// by '|')
	while (args[l] != NULL){
		if (strcmp(args[l],"|") == 0){
			num_cmds++;
		}
		l++;
	}
	num_cmds++;
	
	// Main loop of this method. For each command between '|', the
	// pipes will be configured and standard input and/or output will
	// be replaced. Then it will be executed
	while (args[j] != NULL && end != 1){
		k = 0;
		// We use an auxiliary array of pointers to store the command
		// that will be executed on each iteration
		while (strcmp(args[j],"|") != 0){
			command[k] = args[j];
			j++;	
			if (args[j] == NULL){
				// 'end' variable used to keep the program from entering
				// again in the loop when no more arguments are found
				end = 1;
				k++;
				break;
			}
			k++;
		}
		// Last position of the command will be NULL to indicate that
		// it is its end when we pass it to the exec function
		command[k] = NULL;
		j++;		
		
		// Depending on whether we are in an iteration or another, we
		// will set different descriptors for the pipes inputs and
		// output. This way, a pipe will be shared between each two
		// iterations, enabling us to connect the inputs and outputs of
		// the two different commands.
		if (i % 2 != 0){
			pipe(filedes); // for odd i
		}else{
			pipe(filedes2); // for even i
		}
		
		pid=fork();
		
		if(pid==-1){			
			if (i != num_cmds - 1){
				if (i % 2 != 0){
					close(filedes[1]); // for odd i
				}else{
					close(filedes2[1]); // for even i
				} 
			}			
			printf("Child process could not be created\n");
			return;
		}
		if(pid==0){
			// If we are in the first command
			if (i == 0){
				dup2(filedes2[1], STDOUT_FILENO);
			}
			// If we are in the last command, depending on whether it
			// is placed in an odd or even position, we will replace
			// the standard input for one pipe or another. The standard
			// output will be untouched because we want to see the 
			// output in the terminal
			else if (i == num_cmds - 1){
				if (num_cmds % 2 != 0){ // for odd number of commands
					dup2(filedes[0],STDIN_FILENO);
				}else{ // for even number of commands
					dup2(filedes2[0],STDIN_FILENO);
				}
			// If we are in a command that is in the middle, we will
			// have to use two pipes, one for input and another for
			// output. The position is also important in order to choose
			// which file descriptor corresponds to each input/output
			}else{ // for odd i
				if (i % 2 != 0){
					dup2(filedes2[0],STDIN_FILENO); 
					dup2(filedes[1],STDOUT_FILENO);
				}else{ // for even i
					dup2(filedes[0],STDIN_FILENO); 
					dup2(filedes2[1],STDOUT_FILENO);					
				} 
			}
			
			if (execvp(command[0],command)==err){
				kill(getpid(),SIGTERM);
			}		
		}
				
		// CLOSING DESCRIPTORS ON PARENT
		if (i == 0){
			close(filedes2[1]);
		}
		else if (i == num_cmds - 1){
			if (num_cmds % 2 != 0){					
				close(filedes[0]);
			}else{					
				close(filedes2[0]);
			}
		}else{
			if (i % 2 != 0){					
				close(filedes2[0]);
				close(filedes[1]);
			}else{					
				close(filedes[0]);
				close(filedes2[1]);
			}
		}
				
		waitpid(pid,NULL,0);
				
		i++;	
	}
}

void spaceCommander(char ** args){
	
	int numTokens = 0;
	
	int isFileDirection_stdin = 0;
	int isFileDirection_stdin_Index=-1;
	int isFileDirection_stdout = 0;
	int isFileDirection_stdout_Index=-1;
	int isFileDirection_stderr = 0;
	int isFileDirection_stderr_Index=-1;
	
	int isBackgroundTask = 0;
	int isBackgroundTask_Index = -1;
	
	int isPipeTask = 0;
	int isPipeTask_Index=-1;
	
	int isBG=0;
	int isFG=0;
	int isCustomCommand=0;
	
	while(args[numTokens] != NULL){
		if(_debug==1){
			printf("SPACECOMMANDER: Command Handler arg %d is %s\n",numTokens,args[numTokens]);
		}
		if(strcmp(args[numTokens],"<") == 0){
			if(_debug==1){
				printf("SPACECOMMANDER: This is File Redirection to STDIN\n");
			}
			isFileDirection_stdin = 1;
			isFileDirection_stdin_Index = numTokens;
		}
		if (strcmp(args[numTokens],">") == 0){
			if(_debug==1){
				printf("SPACECOMMANDER: This is File Redirection to STDOUT\n");
			}
			isFileDirection_stdout = 1;
			isFileDirection_stdout_Index = numTokens;
		}
		if(strcmp(args[numTokens],"2>") == 0){
			if(_debug==1){
				printf("SPACECOMMANDER: This is File Redirection to STDERR\n");
			}
			isFileDirection_stderr = 1;
			isFileDirection_stderr_Index = numTokens;
		}
		if(strcmp(args[numTokens],"&") == 0){
			if(_debug==1){
				printf("SPACECOMMANDER: This is Background Task\n");
			}
			isBackgroundTask = 1;
			isFileDirection_stderr_Index = numTokens;
		}
		if(strcmp(args[numTokens],"|") == 0){
			if(_debug==1){
				printf("SPACECOMMANDER: This is Pipe Command\n");
			}
			isPipeTask = 1;
			isPipeTask_Index=numTokens;
		}
		if(strcmp(args[numTokens],"jobs") == 0){
			int status;
			int return_pid;
			if(_debug==1){
				printf("SPACECOMMANDER: This is Jobs Command\n");
			}
			isCustomCommand=1;
			while (_childProcesses != NULL) 
			{ 
				int* childProcess= (_childProcesses->data);
				// printf("LL has %d\n", (int) *childProcess);
				return_pid = waitpid(*childProcess, &status, WUNTRACED); 
	        	if (WIFEXITED(status)) {
	        	  printf("child %d exited, status=%d\n", *childProcess, WEXITSTATUS(status));
	        	} else if (WIFSIGNALED(status)) {
	        	  printf("child %d killed by signal %d\n", *childProcess, WTERMSIG(status));
	        	} else if (WIFSTOPPED(status)) {
	        	  printf("%d stopped by signal %d\n", *childProcess,WSTOPSIG(status));
	        	} else if (WIFCONTINUED(status)) {
	        	  printf("Continuing %d\n",*childProcess);
	        	}
        		_childProcesses = _childProcesses->next; 
			}
		}
		if(strcmp(args[numTokens],"fg") == 0){
			int status;
			if(_debug==1){
				printf("SPACECOMMANDER: This is fg Command\n");
			}
			if(_childProcess>0){
				// printf("SPACECOMMANDER: This is fg Command with Child Process\n");
				kill(_childProcess,SIGCONT);
				// tcsetpgrp (STDIN_FILENO, _childProcess);
				// tcsetattr (STDIN_FILENO, TCSADRAIN, &GBSH_TMODES);
				// tcsetpgrp(STDIN_FILENO, _childProcess);  
				// tcsetpgrp(GBSH_PGID, _childProcess);
				// tcsetpgrp(STDIN_FILENO, getpgid(_childProcess));
				// tcsetattr (STDIN_FILENO, TCSADRAIN, &GBSH_TMODES);
				// waitpid (_childProcess, &status, WUNTRACED);

				// tcsetpgrp(STDIN_FILENO, GBSH_PGID);  
				// tcgetattr(STDIN_FILENO, &GBSH_TMODES);

			}
			isFG = 1;
		}
		if(strcmp(args[numTokens],"bg") == 0){
			if(_debug==1){
				printf("SPACECOMMANDER: This is bg Command\n");
			}
			if(_childProcess>0){
				kill(_childProcess,SIGCONT);
			}
			isBG = 1;
		}
		if(strcmp(args[numTokens],"getChildProcess") == 0){
			printf("%d\n", _childProcess);
			isCustomCommand=1;
		}
		if(strcmp(args[numTokens],"DEBUG_ON") == 0){
			_debug=1;
			isCustomCommand=1;
		}
		if(strcmp(args[numTokens],"DEBUG_OFF") == 0){
			_debug=0;
			isCustomCommand=1;
		}
		numTokens++;
	}
	if(isFileDirection_stdin ==1 || isFileDirection_stdout==1 || isFileDirection_stderr==1 || isPipeTask==1){
		if(isFileDirection_stdin==1 || isFileDirection_stdout==1 || isFileDirection_stderr==1 ){
			if(isFileDirection_stdin==1){
				fileRedirectCommand(args,isFileDirection_stdin_Index,1, isBackgroundTask);
			} 
			if(isFileDirection_stdout==1){
				fileRedirectCommand(args,isFileDirection_stdout_Index,2, isBackgroundTask);
			}
			if(isFileDirection_stderr==1 ){
				fileRedirectCommand(args,isFileDirection_stderr_Index,3, isBackgroundTask);
			}
		}
		if(isPipeTask==1){
			if(_debug==1){
				printf("SPACECOMMANDER: This is Pipe Command \n");
			}
			launchPipeCommand(args,isPipeTask_Index, isBackgroundTask_Index, isBackgroundTask);
		}
	}else if(isBG!=1 && isFG!=1 && isCustomCommand !=1){
		if(_debug==1){
			printf("SPACECOMMANDER: Execute Command \n");
		}
		launchSpaceCommand(args,isBackgroundTask_Index,isBackgroundTask);
	}
}


int main(void) {
	init();
	char *inString;
	int numTokens;
	char *str1, *str2, *token, *subtoken;
    char *saveptr1, *saveptr2;
    int j;
    if (signal(SIGINT, sig_handler) == SIG_ERR)   
		printf("signal(SIGINT) error\n");
	if (signal(SIGTSTP, sig_handler) == SIG_ERR)   
		printf("signal(SIGTSTP) error\n");
	while(inString = readline("# ")){
		char *tokens[1000];
		memset(tokens, 0, sizeof(tokens));
		if(strtok(inString,"\n\t") != NULL ){
			if(_debug==1){
				printf("MAIN: Command Received: %s \n", inString);
			}
			numTokens = 0;
			for (j = 0, str1 = inString; ; j++, str1 = NULL) {
	        	token = strtok_r(str1, " \n\t", &saveptr1);
	        	if (token == NULL)
	            	break;
	    		for (str2 = token; ; str2 = NULL) {
	            	subtoken = strtok_r(str2," \n\t", &saveptr2);
	            	if (subtoken == NULL)
	                	break;
	            	tokens[numTokens]=subtoken;
	            	numTokens++;
	        	}
	    	}
			numTokens = 0;
			while(tokens[numTokens] != NULL){ 
				if(_debug==1){
					printf("MAIN: Token %d is %s\n",numTokens,tokens[numTokens]);
				}
				numTokens++;
			}
			spaceCommander(tokens);
		}
	}
}

