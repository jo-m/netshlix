HEADERS = rtp.h rtp_jpeg.h rfc2435.h fakesp.h
OBJECTS = rtp.o rtp_jpeg.o rfc2435.o linux_main.o

default: linux_main

CC = gcc
CFLAGS = -g -O2 -std=gnu17 -Wall -Werror -Wextra -Wpedantic -Wshadow -Wsign-compare -Wunreachable-code
CFLAGS_CLANG = -Wno-gnu-zero-variadic-macro-arguments -Wno-strict-prototypes
SAN = address # Can also use memory, undefined
CFLAGS_SAN = -fsanitize=$(SAN) -fno-omit-frame-pointer
LDFLAGS =
LDFLAGS_SAN = -fsanitize=$(SAN)

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

linux_main: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

linux_main_san: CC = clang-15
linux_main_san: CFLAGS += $(CFLAGS_CLANG) $(CFLAGS_SAN)
linux_main_san: LDFLAGS += $(LDFLAGS_SAN)
linux_main_san: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	-rm -f $(OBJECTS)
	-rm -f linux_main
	-rm -f linux_main_san
