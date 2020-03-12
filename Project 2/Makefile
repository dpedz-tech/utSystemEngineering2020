default: all

# all: simpleTerm newSimpleTerm basicTerm gnuTerm
all:yash

clean:
	# rm -f simpleTerm
	# rm -f newSimpleTerm
	# rm -f basicTerm
	# rm -f gnuTerm
	rm -f yash
	# rm -f *.txt

# simpleTerm: simpleTerm.c
# 	gcc -o simpleTerm simpleTerm.c -lreadline 

# newSimpleTerm: newSimpleTerm.c
# 	gcc -o newSimpleTerm newSimpleTerm.c -lreadline 
	
# basicTerm: terminal.c
# 	gcc -o basicTerm terminal.c -lreadline 

# gnuTerm: gnuTerm.c
# 	gcc -o gnuTerm gnuTerm.c -lreadline 

yash: newSimpleTerm.c
	gcc -o yash newSimpleTerm.c -lreadline 
