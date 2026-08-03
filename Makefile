CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -I include -O2
CFLAGS_LTO = $(CFLAGS) -flto
LDFLAGS = -lm
AR      = ar

SRC_DIR   = src
TEST_DIR  = tests
BUILD_DIR = build

SRCS_CORE = $(SRC_DIR)/karubyte.c $(SRC_DIR)/distribution.c $(SRC_DIR)/stochastic.c
SRCS_MID  = $(SRCS_CORE) $(SRC_DIR)/memory.c
SRCS_ALL  = $(SRCS_MID) $(SRC_DIR)/collapse.c $(SRC_DIR)/branch.c \
            $(SRC_DIR)/pruner.c $(SRC_DIR)/runtime.c
SRCS_LANG = $(SRC_DIR)/lexer.c $(SRC_DIR)/parser.c $(SRC_DIR)/ast.c \
            $(SRC_DIR)/interpreter.c
SRCS_COMP = $(SRC_DIR)/ir.c $(SRC_DIR)/typecheck.c $(SRC_DIR)/vm.c \
            $(SRC_DIR)/codegen.c $(SRC_DIR)/specialize.c
SRCS_FULL = $(SRCS_ALL) $(SRCS_LANG) $(SRCS_COMP)

LIB_OBJS_SRC = $(SRCS_ALL)

# ── Targets ─────────────────────────────────────────

.PHONY: all test clean quanti libquanti install-lib

all: quanti libquanti test

$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

libquanti: $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/karubyte.c -o $(BUILD_DIR)/karubyte.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/distribution.c -o $(BUILD_DIR)/distribution.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/stochastic.c -o $(BUILD_DIR)/stochastic.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/memory.c -o $(BUILD_DIR)/memory.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/collapse.c -o $(BUILD_DIR)/collapse.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/branch.c -o $(BUILD_DIR)/branch.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/pruner.c -o $(BUILD_DIR)/pruner.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/runtime.c -o $(BUILD_DIR)/runtime.o
	$(AR) rcs $(BUILD_DIR)/libquanti.a \
		$(BUILD_DIR)/karubyte.o $(BUILD_DIR)/distribution.o \
		$(BUILD_DIR)/stochastic.o $(BUILD_DIR)/memory.o \
		$(BUILD_DIR)/collapse.o $(BUILD_DIR)/branch.o \
		$(BUILD_DIR)/pruner.o $(BUILD_DIR)/runtime.o

quanti: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/quanti.exe \
		$(SRC_DIR)/main.c $(SRCS_FULL) $(LDFLAGS)

quanti-lto: $(BUILD_DIR)
	$(CC) $(CFLAGS_LTO) -o $(BUILD_DIR)/quanti.exe \
		$(SRC_DIR)/main.c $(SRCS_FULL) $(LDFLAGS)

test: $(BUILD_DIR) test_karubyte test_memory test_collapse test_branch \
	test_runtime test_stochastic test_lexer test_parser test_interpreter test_ir
	@echo.
	@echo === All test suites ===
	@$(BUILD_DIR)\test_karubyte.exe
	@$(BUILD_DIR)\test_memory.exe
	@$(BUILD_DIR)\test_collapse.exe
	@$(BUILD_DIR)\test_branch.exe
	@$(BUILD_DIR)\test_runtime.exe
	@$(BUILD_DIR)\test_stochastic.exe
	@$(BUILD_DIR)\test_lexer.exe
	@$(BUILD_DIR)\test_parser.exe
	@$(BUILD_DIR)\test_interpreter.exe
	@$(BUILD_DIR)\test_ir.exe

test_stochastic: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_stochastic.exe \
		$(TEST_DIR)/test_stochastic.c $(SRC_DIR)/stochastic.c $(SRC_DIR)/distribution.c $(LDFLAGS)

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

test_lexer: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_lexer.exe \
		$(TEST_DIR)/test_lexer.c $(SRC_DIR)/lexer.c $(LDFLAGS)

test_parser: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_parser.exe \
		$(TEST_DIR)/test_parser.c $(SRC_DIR)/parser.c $(SRC_DIR)/lexer.c $(SRC_DIR)/ast.c $(LDFLAGS)

test_interpreter: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_interpreter.exe \
		$(TEST_DIR)/test_interpreter.c $(SRCS_FULL) $(LDFLAGS)

test_ir: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_ir.exe \
		$(TEST_DIR)/test_ir.c $(SRCS_FULL) $(LDFLAGS)

benchmark_aot: $(BUILD_DIR) quanti
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/benchmark_aot.exe \
		$(TEST_DIR)/benchmark_aot.c $(SRCS_FULL) $(LDFLAGS)
	$(BUILD_DIR)/benchmark_aot.exe

clean:
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
