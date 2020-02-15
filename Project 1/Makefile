default: all

all: daleTerm

clean:
	rm -f daleTerm
	rm -f execTerm
	rm -f basicTerm
	rm -f gnuTerm

daleTerm: simpleTerm.c
	gcc -o daleTerm simpleTerm.c -lreadline 

exec2: exec2.c
	gcc -o execTerm exec2.c -lreadline 
	
basicTerm: terminal.c
	gcc -o basicTerm terminal.c -lreadline 

gnuTerm: gnuTerm.c
	gcc -o gnuTerm gnuTerm.c -lreadline 
