
CC		= gcc
CFLAGS	= -Wall -g -O0 -D_GNU_SOURCE -fPIC
LDFLAGS = -lunwind -lunwind-$(shell uname -m)
PROGS	= chals libpoem.so libsolver.so

all: $(PROGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

chals: chals.o libpoem.so
	$(CC) -o $@ $^ $(LDFLAGS)

%.so: %.o
	$(CC) -o $@ -shared $^ $(LDFLAGS)

test:
	LD_LIBRARY_PATH=`pwd` LD_PRELOAD=./libsolver.so ./chals

run:
	LD_LIBRARY_PATH=`pwd` ./chals

cleanup:
	rm -f *.o

clean:
	rm -f $(PROGS) *.o
	rm -rf __pycache__

