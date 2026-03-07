CC=gcc
CFLAGS=-Wall -Wextra
OUT_DIR=dist

# default server name (can be overridden)
TARGET?=server

SRC=$(TARGET).c
EXTRA_SRCS=

# detect optional sources
ifeq ($(wildcard chttp.c),chttp.c)
EXTRA_SRCS+=chttp.c
CFLAGS+=-lpthread
endif

ifeq ($(wildcard cJSON.c),cJSON.c)
EXTRA_SRCS+=cJSON.c
endif

# detect PAM usage
PAM:=$(shell grep -q pam_appl.h $(SRC) && echo yes)
ifeq ($(PAM),yes)
CFLAGS+=-lpam
endif

OUT=$(OUT_DIR)/$(TARGET)

all: build

build:
	mkdir -p $(OUT_DIR)
	@echo "Compiling..."
	/usr/bin/time -f "compiled in %es" \
	$(CC) $(SRC) $(EXTRA_SRCS) $(CFLAGS) -o $(OUT)

run: build
	@echo "Running $(OUT)"
	/usr/bin/time -f "ran in %es" ./$(OUT)

clean:
	rm -rf $(OUT_DIR)

.PHONY: all build run clean
