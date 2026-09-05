CC ?= gcc
UNAME_S := $(shell uname -s)

CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2 -Wnull-dereference -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D_DARWIN_C_SOURCE
endif

INCLUDE_DIRS = -Iinclude

SRC_DIRS = src src/runtime src/process src/namespaces src/utils src/config
SRCS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
LIB_SRCS = $(filter-out src/main.c, $(SRCS))
OBJS = $(patsubst src/%.c,build/%.o,$(SRCS))
LIB_OBJS = $(patsubst src/%.c,build/%.o,$(LIB_SRCS))

TARGET = bin/myrun
TEST_PROCESS_BIN = bin/test_process
TEST_PID_BIN = bin/test_pid
TEST_UTS_BIN = bin/test_uts

# Build modes
.PHONY: all debug release asan clean test test-linux docker-build

all: release

debug: CFLAGS += -g3 -O0 -DDEBUG
debug: $(TARGET)

release: CFLAGS += -O2
release: $(TARGET)

asan: CFLAGS += -fsanitize=address,undefined -g3 -O1 -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address,undefined
asan: $(TARGET) $(TEST_PROCESS_BIN) $(TEST_PID_BIN) $(TEST_UTS_BIN)

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

# Tests compilation
build/tests/process/%.o: tests/process/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -Itests -c $< -o $@

build/tests/namespaces/%.o: tests/namespaces/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -Itests -c $< -o $@

$(TEST_PROCESS_BIN): $(LIB_OBJS) build/tests/process/test_process.o | bin
	$(CC) $^ $(LDFLAGS) -o $@

$(TEST_PID_BIN): $(LIB_OBJS) build/tests/namespaces/test_pid.o | bin
	$(CC) $^ $(LDFLAGS) -o $@

$(TEST_UTS_BIN): $(LIB_OBJS) build/tests/namespaces/test_uts.o | bin
	$(CC) $^ $(LDFLAGS) -o $@

test: $(TEST_PROCESS_BIN) $(TEST_PID_BIN) $(TEST_UTS_BIN)
	@echo "=== Running Process Test Suite ==="
	@./$(TEST_PROCESS_BIN)
	@echo ""
	@echo "=== Running PID Namespace Test Suite ==="
	@./$(TEST_PID_BIN)
	@echo ""
	@echo "=== Running UTS Namespace Test Suite ==="
	@./$(TEST_UTS_BIN)

# Linux Docker Testing
docker-build:
	docker build -t myrun-dev -f Dockerfile.dev .

test-linux: docker-build
	docker run --rm --privileged -v "$$(pwd):/workspace" -w /workspace myrun-dev bash -c "make clean && make asan test"

clean:
	rm -rf build bin
