HEADERS = rtp.h rtp_jpeg.h rfc2435.h fakesp.h
OBJECTS = rtp.o rtp_jpeg.o rfc2435.o linux_main.o

default: linux_main

CC = gcc-12
CFLAGS = -g -O2 -std=gnu17 -Wall -Werror -Wextra -Wpedantic -Wshadow -Wsign-compare -Wunreachable-code
LDFLAGS =

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

linux_main: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	-rm -f $(OBJECTS)
	-rm -f linux_main
