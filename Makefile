CC = gcc
CFLAGS = -g -I -std=gnu99 -lpthread -lm -lrt -std=c99 -D_POSIX_C_SOURCE=200809 -D_SVID_SOURCE
all: oss process

oss: config.h oss.c
	$(CC) -o $@ $^ $(CFLAGS)

uprocess: config.h process.c
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm oss process logfile.txt