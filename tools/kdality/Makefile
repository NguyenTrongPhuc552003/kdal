CC     ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE \
          -DKDAL_VERSION_STRING=\"$(shell cat ../../VERSION 2>/dev/null || echo 0.1.0-dev)\"

# Path to the compiler library built by compiler/Makefile
KDALC_DIR  := ../../compiler
KDALC_LIB  := $(KDALC_DIR)/libkdalc.a
KDALC_INC  := -I$(KDALC_DIR)/include

.PHONY: all clean compiler_lib

all: compiler_lib kdality

compiler_lib:
	$(MAKE) -C $(KDALC_DIR) lib

kdality: main.o kdalctl.o compile.o init.o dtgen.o simulate.o testgen.o lint.o $(KDALC_LIB)
	$(CC) $(CFLAGS) -o $@ main.o kdalctl.o compile.o init.o dtgen.o simulate.o testgen.o lint.o $(KDALC_LIB)

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

kdalctl.o: kdalctl.c
	$(CC) $(CFLAGS) -c -o $@ $<

compile.o: compile.c
	$(CC) $(CFLAGS) $(KDALC_INC) -c -o $@ $<

init.o: init.c templates.h
	$(CC) $(CFLAGS) -c -o $@ $<

dtgen.o: dtgen.c
	$(CC) $(CFLAGS) -c -o $@ $<

simulate.o: simulate.c
	$(CC) $(CFLAGS) -c -o $@ $<

testgen.o: testgen.c
	$(CC) $(CFLAGS) -c -o $@ $<

lint.o: lint.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f kdality *.o
