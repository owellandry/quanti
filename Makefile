CC      = gcc
# Hexagonal micromodules: domain | application | adapters/{cli,frontend,backend}
INCLUDES = -I domain -I application -I adapters/frontend -I adapters/backend
CFLAGS  = -Wall -Wextra -std=c11 $(INCLUDES) -O2
CFLAGS_LTO = $(CFLAGS) -flto
LDFLAGS = -lm
AR      = ar

DOMAIN_DIR = domain
APP_DIR    = application
FE_DIR     = adapters/frontend
BE_DIR     = adapters/backend
CLI_DIR    = adapters/cli
TEST_DIR   = tests
BUILD_DIR  = build

# ── Domain (hexagon core) ───────────────────────────
SRCS_CORE = $(DOMAIN_DIR)/karubyte.c $(DOMAIN_DIR)/distribution.c $(DOMAIN_DIR)/stochastic.c
SRCS_MID  = $(SRCS_CORE) $(DOMAIN_DIR)/memory.c
SRCS_DOMAIN = $(SRCS_MID) $(DOMAIN_DIR)/collapse.c $(DOMAIN_DIR)/branch.c \
              $(DOMAIN_DIR)/pruner.c

# ── Application (ports / use-cases) ─────────────────
SRCS_APP  = $(APP_DIR)/runtime.c $(APP_DIR)/ir.c $(APP_DIR)/typecheck.c \
            $(APP_DIR)/specialize.c

# ── Adapters ────────────────────────────────────────
SRCS_FE   = $(FE_DIR)/lexer.c $(FE_DIR)/parser.c $(FE_DIR)/ast.c \
            $(FE_DIR)/interpreter.c
SRCS_BE   = $(BE_DIR)/vm.c $(BE_DIR)/codegen.c

SRCS_ALL  = $(SRCS_DOMAIN) $(APP_DIR)/runtime.c
SRCS_FULL = $(SRCS_DOMAIN) $(SRCS_APP) $(SRCS_FE) $(SRCS_BE)

# ── Targets ─────────────────────────────────────────

.PHONY: all test clean quanti libquanti quanti-lto benchmark_aot

all: quanti libquanti test

$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

libquanti: $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(DOMAIN_DIR)/karubyte.c -o $(BUILD_DIR)/karubyte.o
	$(CC) $(CFLAGS) -c $(DOMAIN_DIR)/distribution.c -o $(BUILD_DIR)/distribution.o
	$(CC) $(CFLAGS) -c $(DOMAIN_DIR)/stochastic.c -o $(BUILD_DIR)/stochastic.o
	$(CC) $(CFLAGS) -c $(DOMAIN_DIR)/memory.c -o $(BUILD_DIR)/memory.o
	$(CC) $(CFLAGS) -c $(DOMAIN_DIR)/collapse.c -o $(BUILD_DIR)/collapse.o
	$(CC) $(CFLAGS) -c $(DOMAIN_DIR)/branch.c -o $(BUILD_DIR)/branch.o
	$(CC) $(CFLAGS) -c $(DOMAIN_DIR)/pruner.c -o $(BUILD_DIR)/pruner.o
	$(CC) $(CFLAGS) -c $(APP_DIR)/runtime.c -o $(BUILD_DIR)/runtime.o
	$(AR) rcs $(BUILD_DIR)/libquanti.a \
		$(BUILD_DIR)/karubyte.o $(BUILD_DIR)/distribution.o \
		$(BUILD_DIR)/stochastic.o $(BUILD_DIR)/memory.o \
		$(BUILD_DIR)/collapse.o $(BUILD_DIR)/branch.o \
		$(BUILD_DIR)/pruner.o $(BUILD_DIR)/runtime.o

quanti: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/quanti.exe \
		$(CLI_DIR)/main.c $(SRCS_FULL) $(LDFLAGS)

quanti-lto: $(BUILD_DIR)
	$(CC) $(CFLAGS_LTO) -o $(BUILD_DIR)/quanti.exe \
		$(CLI_DIR)/main.c $(SRCS_FULL) $(LDFLAGS)

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
		$(TEST_DIR)/test_stochastic.c $(DOMAIN_DIR)/stochastic.c $(DOMAIN_DIR)/distribution.c $(LDFLAGS)

test_karubyte: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_karubyte.exe \
		$(TEST_DIR)/test_karubyte.c $(SRCS_CORE) $(LDFLAGS)

test_memory: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_memory.exe \
		$(TEST_DIR)/test_memory.c $(SRCS_MID) $(LDFLAGS)

test_collapse: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_collapse.exe \
		$(TEST_DIR)/test_collapse.c $(DOMAIN_DIR)/collapse.c $(SRCS_MID) $(LDFLAGS)

test_branch: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_branch.exe \
		$(TEST_DIR)/test_branch.c $(DOMAIN_DIR)/branch.c $(SRCS_MID) $(LDFLAGS)

test_runtime: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_runtime.exe \
		$(TEST_DIR)/test_runtime.c $(SRCS_ALL) $(LDFLAGS)

test_lexer: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_lexer.exe \
		$(TEST_DIR)/test_lexer.c $(FE_DIR)/lexer.c $(LDFLAGS)

test_parser: $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/test_parser.exe \
		$(TEST_DIR)/test_parser.c $(FE_DIR)/parser.c $(FE_DIR)/lexer.c $(FE_DIR)/ast.c $(LDFLAGS)

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
