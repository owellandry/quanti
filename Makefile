CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -I include
LDFLAGS = -lm

SRC_DIR   = src
TEST_DIR  = tests
BUILD_DIR = build

SRCS_CORE = $(SRC_DIR)/karubyte.c $(SRC_DIR)/distribution.c
SRCS_MID  = $(SRCS_CORE) $(SRC_DIR)/memory.c
SRCS_ALL  = $(SRCS_MID) $(SRC_DIR)/collapse.c $(SRC_DIR)/branch.c \
            $(SRC_DIR)/pruner.c $(SRC_DIR)/runtime.c

# ── Targets ─────────────────────────────────────────

.PHONY: all test clean

all: test

$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

test: $(BUILD_DIR) test_karubyte test_memory test_collapse test_branch test_runtime
	@echo.
	@echo === All test suites ===
	@$(BUILD_DIR)\test_karubyte.exe
	@$(BUILD_DIR)\test_memory.exe
	@$(BUILD_DIR)\test_collapse.exe
	@$(BUILD_DIR)\test_branch.exe
	@$(BUILD_DIR)\test_runtime.exe

test_karubyte: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_karubyte.exe \
		$(TEST_DIR)/test_karubyte.c $(SRCS_CORE) $(LDFLAGS)

test_memory: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_memory.exe \
		$(TEST_DIR)/test_memory.c $(SRCS_MID) $(LDFLAGS)

test_collapse: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_collapse.exe \
		$(TEST_DIR)/test_collapse.c $(SRC_DIR)/collapse.c $(SRCS_MID) $(LDFLAGS)

test_branch: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_branch.exe \
		$(TEST_DIR)/test_branch.c $(SRC_DIR)/branch.c $(SRCS_MID) $(LDFLAGS)

test_runtime: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_runtime.exe \
		$(TEST_DIR)/test_runtime.c $(SRCS_ALL) $(LDFLAGS)

clean:
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
