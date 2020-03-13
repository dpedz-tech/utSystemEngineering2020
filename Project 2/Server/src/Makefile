CC=gcc

LDFLAGS=-pthread
ODIR=obj
_OBJ = yashd.o yash_thread.o yash.o
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

IDIR =../include
CFLAGS=-I$(IDIR) -std=gnu99 ld--version
#DEPS = $(patsubst %, $(IDIR)/%)

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LDFLAGS)

yashd : $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm yashd yashd.o yash_thread.o
