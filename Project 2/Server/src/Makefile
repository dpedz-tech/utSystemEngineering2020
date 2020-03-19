CC=gcc

LDFLAGS=-pthread -lutil -std=gnu99
ODIR=obj
_OBJ = yashd.o yash_thread.o yash.o
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

IDIR =../include
CFLAGS=-I$(IDIR)
#DEPS = $(patsubst %, $(IDIR)/%)

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LDFLAGS)

yashd : $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm yashd obj/yashd.o obj/yash_thread.o obj/yash.o
