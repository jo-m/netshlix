HEADERS = rtp.h rtp_jpeg.h rfc2435.h fakesp.h
OBJECTS = rtp.o rtp_jpeg.o rfc2435.o

default: linux_main

CC = gcc
CFLAGS = -g -O2 -std=gnu17 -Wall -Werror -Wextra -Wpedantic -Wshadow -Wsign-compare -Wunreachable-code -fstack-usage
LDFLAGS =

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

linux_main: $(OBJECTS) linux_main.o Makefile
	$(CC) $(OBJECTS) linux_main.o $(LDFLAGS) -o $@

# Clang/Sanitizers

CFLAGS_CLANG = -Wno-gnu-zero-variadic-macro-arguments -Wno-strict-prototypes

SAN = address # Can also use 'memory', 'undefined'.
CFLAGS_SAN = -fsanitize=$(SAN) -fno-omit-frame-pointer
LDFLAGS_SAN = -fsanitize=$(SAN)

linux_main_san: CC = clang-15
linux_main_san: CFLAGS += $(CFLAGS_CLANG) $(CFLAGS_SAN)
linux_main_san: LDFLAGS += $(LDFLAGS_SAN)
linux_main_san: $(OBJECTS) linux_main.o Makefile
	$(CC) $(OBJECTS) linux_main.o $(LDFLAGS) -o $@

# Fuzz

CFLAGS_FUZZ = -DNDEBUG
LDFLAGS_FUZZ = -lpcap

linux_fuzztarget_pcap: CC = afl-cc
linux_fuzztarget_pcap: CFLAGS += $(CFLAGS_CLANG) $(CFLAGS_FUZZ)
linux_fuzztarget_pcap: LDFLAGS += $(LDFLAGS_FUZZ)
linux_fuzztarget_pcap: $(OBJECTS) linux_fuzztarget_pcap.o Makefile
	echo $(LDFLAGS)
	$(CC) $(OBJECTS) linux_fuzztarget_pcap.o $(LDFLAGS) -o $@

# Phony
.PHONY: clean
clean:
	-rm -f $(OBJECTS) linux_main.o linux_fuzztarget_pcap.o
	-rm -f linux_main
	-rm -f linux_main_san
	-rm -f linux_fuzztarget_pcap
