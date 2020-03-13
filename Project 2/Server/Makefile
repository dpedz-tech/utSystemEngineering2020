CC=gcc

LDFLAGS=-pthread
ODIR=obj
_OBJ = yashd.o yash_thread.o
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

IDIR =../include
CFLAGS=-I$(IDIR)
DEPS = $(patsubst %, $(IDIR)/%)

$(ODIR)/%.o: %.c $(DEPS)
	@echo "Here0"
	$(CC) -c -o $@ $< $(CFLAGS) $(LDFLAGS)

yashd : $(OBJ)
	@echo "Here1"
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm yashd yashd.o yash_thread.o
