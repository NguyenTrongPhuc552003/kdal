# SPDX-License-Identifier: GPL-2.0-only
#
# KDAL compiler (kdalc) build system
#
# Targets:
#   all         — build libkdalc.a and kdalc binary
#   lib         — build libkdalc.a only  (for kdality link)
#   kdalc       — build standalone kdalc binary
#   clean       — remove build artefacts
#   install     — install kdalc to $(DESTDIR)/usr/local/bin
#

CC      ?= gcc
AR      ?= ar
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic \
           -g -O2 \
           -DKDAL_VERSION_STRING=\"$(shell cat ../VERSION 2>/dev/null || echo 0.1.0-dev)\"
ARFLAGS := rcs

SRCDIR  := .
INCDIR  := include
OBJDIR  := .obj

LIB_SRCS := lexer.c parser.c sema.c codegen.c
BIN_SRCS := main.c

LIB_OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(LIB_SRCS))
BIN_OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(BIN_SRCS))

LIB_OUT  := libkdalc.a
BIN_OUT  := kdalc

.PHONY: all lib kdalc clean install

all: lib kdalc

lib: $(LIB_OUT)

kdalc: $(BIN_OUT)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(LIB_OUT): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(BIN_OUT): $(LIB_OBJS) $(BIN_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(OBJDIR) $(LIB_OUT) $(BIN_OUT)

install: $(BIN_OUT)
	install -Dm755 $(BIN_OUT) $(DESTDIR)/usr/local/bin/$(BIN_OUT)

# Ensure header dependencies are tracked
$(OBJDIR)/lexer.o:   $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/codegen.h
$(OBJDIR)/parser.o:  $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/codegen.h
$(OBJDIR)/sema.o:    $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/codegen.h
$(OBJDIR)/codegen.o: $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/codegen.h
$(OBJDIR)/main.o:    $(INCDIR)/token.h $(INCDIR)/ast.h $(INCDIR)/codegen.h
