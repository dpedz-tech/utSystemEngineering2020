#Project 1 Simple Shell

Here is the complete list of features you must implement:  

File redirection
with creation of files if they don't exist for output redirection
fail command if input redirection (a file) does not exist
< will replace stdin with the file that is the next token
> will replace stdout with the file that is the next token
2> will replace stderr with the file that is the next token
A command can have all three (or a subset) of the redirection symbols 

Piping
| separates two commands
The left command will have stdout replaced with the input to a pipe
The right command will have stdin replaced with the output from the same pipe
Children within the same pipeline will be in one process group for the pipeline
Children within the same pipeline will be started and stopped simultaneously
Only one | must be present in each pipeline

Signals (SIGINT, SIGTSTP, SIGCHLD)
Ctrl-c must quit current foreground process (if one exists)  and not the shell and should not print the process (unlike bash)
Ctrl-z must send SIGTSTP to the current foreground process and should not print the process (unlike bash)
The shell will not be stopped on SIGTSTP

Job control
Background jobs using &
fg must send SIGCONT to the most recent background or stopped process, print the process to stdout , and wait for completion
bg must send SIGCONT to the most recent stopped process, print the process to stdout in the jobs format, and not wait for completion (as if &)
jobs will print the job control table similar to bash. HOWEVER there are important differences between your yash shell’s output and bash’s output for the jobs command! Please see the FAQ below.
 with a [<jobnum>]
 a + or - indicating  the current job. Which job would be run with an fg,  is indicated with a + and, all others with a -
a “Stopped” or “Running” indicating the status of the process
and finally the original command
e.g. [1] - Running       sleep 5 &
       [2] - Stopped        sleep 5
       [3] + Running       long_running_command | grep > output.txt &
Terminated background jobs will be printed after the newline character sent on stdin with a Done in place of the Stopped or Running.
A Job is all children within one pipeline as defined above

Misc
Children must inherit the environment from the parent
Must search the PATH environment variable for every executable
All child processes will be dead on exit
The prompt must be printed as a ‘# ‘ (hashtag-sign with a space after it) before accepting user input.
Make sure that your shell doesn’t segfault (for whatever reason) when you run something like this because our grading script uses the ‘echo’ command with the ‘-e’ flag
Echo -e ‘hi’
ls
