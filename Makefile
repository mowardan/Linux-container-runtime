CC ?= gcc
UNAME_S := $(shell uname -s)

CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2 -Wnull-dereference -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D_DARWIN_C_SOURCE
endif

INCLUDE_DIRS = -Iinclude

SRC_DIRS = src src/runtime src/process src/utils src/config
SRCS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJS = $(patsubst src/%.c,build/%.o,$(SRCS))

TARGET = bin/myrun
TEST_BIN = bin/test_runner

# Build modes
.PHONY: all debug release asan clean test test-linux docker-build

all: release

debug: CFLAGS += -g3 -O0 -DDEBUG
debug: $(TARGET)

release: CFLAGS += -O2
release: $(TARGET)

asan: CFLAGS += -fsanitize=address,undefined -g3 -O1 -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address,undefined
asan: $(TARGET) $(TEST_BIN)

$(TARGET): $(OBJS) | bin
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# Object compilation
build/%.o: src/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

build:
	mkdir -p build

bin:
	mkdir -p bin

# Tests
TEST_SRCS = $(filter-out src/main.c, $(SRCS)) tests/process/test_process.c
TEST_OBJS = $(patsubst %.c,build/test/%.o,$(TEST_SRCS))

build/test/%.o: %.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -Itests -c $< -o $@

$(TEST_BIN): $(TEST_OBJS) | bin
	$(CC) $(TEST_OBJS) $(LDFLAGS) -o $@

test: $(TEST_BIN)
	@echo "=== Running MyRun Test Suite ==="
	@./$(TEST_BIN)

# Linux Docker Testing
docker-build:
	docker build -t myrun-dev -f Dockerfile.dev .

test-linux: docker-build
	docker run --rm -v "$$(pwd):/workspace" -w /workspace myrun-dev bash -c "make clean && make asan test"

clean:
	rm -rf build bin
