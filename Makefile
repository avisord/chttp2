CC      := gcc
CFLAGS  := -Wall -Wextra -I src -I lib -I vendor
LDFLAGS := -lpthread -lpam

ifdef TLS
CFLAGS  += -DCHTTP_TLS
LDFLAGS += -lssl -lcrypto
endif

SRC_DIR    := src
LIB_DIR    := lib
VENDOR_DIR := vendor
BUILD_DIR  := build
OUT_DIR    := dist
TARGET     := $(OUT_DIR)/server

SRCS := $(shell find $(SRC_DIR) $(LIB_DIR) $(VENDOR_DIR) -name '*.c')
OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run compdb

all: $(TARGET)

$(TARGET): $(OBJS) | $(OUT_DIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR):
	mkdir -p $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(OUT_DIR)

run: all
	./$(TARGET)

compdb:
	python3 -c "\
import json, os; root=os.getcwd(); srcs=open('/dev/stdin').read().split();\
db=[{'directory':root,'command':'gcc $(CFLAGS) -c '+f+' -o /dev/null','file':os.path.join(root,f)} for f in sorted(srcs)];\
open('compile_commands.json','w').write(json.dumps(db,indent=2))" <<< "$(SRCS)"
