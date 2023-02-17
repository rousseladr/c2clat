CC=gcc
CFLAGS=-O3 -Wall
LDFLAGS=-pthread

EXEC=c2clat.exe
SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

proper :
	rm -f *.o

clean : proper
	rm -f $(EXEC)
