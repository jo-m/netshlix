HEADERS = rtp.h rtp_jpeg.h time.h fakesp.h
OBJECTS = rtp.o rtp_jpeg.o time.o recv.o

default: recv

CFLAGS = -g -std=gnu17 -Wall -Werror -Wextra -Wpedantic -Wshadow -Wsign-compare -Wunreachable-code
LDFLAGS =

%.o: %.c $(HEADERS) Makefile
	gcc $(CFLAGS) -c $< -o $@

recv: $(OBJECTS) Makefile
	gcc $(OBJECTS) $(LDFLAGS) -o $@

clean:
	-rm -f $(OBJECTS)
	-rm -f recv
