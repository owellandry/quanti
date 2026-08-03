#ifndef QUANTI_IR_H
#define QUANTI_IR_H

#include "ast.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Quenti IR — Intermediate representation for QA.
 * Linear bytecode used by the VM and AOT codegen.
 */

typedef enum {
    IR_NOP = 0,
    IR_CONST_I,      /* operand = int immediate */
    IR_CONST_F,      /* uses fimm */
    IR_CONST_S,      /* operand = string table index */
    IR_CONST_B,      /* operand = 0/1 */
    IR_LOAD,         /* operand = local slot */
    IR_STORE,        /* operand = local slot */
    IR_ADD, IR_SUB, IR_MUL, IR_DIV,
    IR_EQ, IR_NEQ, IR_LT, IR_GT, IR_LTE, IR_GTE,
    IR_NEG,
    IR_JMP,          /* operand = absolute ip */
    IR_JMP_IF,       /* pop cond; jump if truthy */
    IR_JMP_IF_NOT,   /* pop cond; jump if falsy */
    IR_PRINT,
    IR_POP,
    IR_CALL,         /* operand = func index; aux = argc */
    IR_RET,
    IR_KARU_SUPER,   /* aux = arity (2 → K, else equal discrete) */
    IR_KARU_AND,
    IR_KARU_OR,
    IR_KARU_NOT,
    IR_MEASURE,      /* operand = MeasureMode */
    IR_P_NORMAL,     /* stack: mean, stddev */
    IR_P_UNIFORM,    /* stack: min, max */
    IR_P_DISCRETE,   /* operand = n probs; next n consts are probs; optional labels */
    IR_FORK_IF,      /* multistate if: operand=else_ip, aux=end_ip (simplified) */
    IR_RUNTIME_CFG,  /* uses aux bits */
    IR_HALT,
} IrOpcode;

typedef struct {
    IrOpcode op;
    int32_t  operand;
    int32_t  aux;
    double   fimm;
    bool     classical;  /* specialization: no karu runtime needed */
    int      line;
} IrInst;

typedef struct {
    char   *name;
    QAType  type;
    int     slot;
} IrLocal;

typedef struct {
    char    *name;
    int      entry_ip;
    int      arity;
    QAType   ret_type;
    int     *param_slots;
} IrFunc;

typedef struct {
    IrInst  *code;
    size_t   count;
    size_t   capacity;

    IrLocal *locals;
    size_t   local_count;
    size_t   local_cap;

    IrFunc  *funcs;
    size_t   func_count;
    size_t   func_cap;

    char   **strings;
    size_t   string_count;
    size_t   string_cap;

    bool     had_error;
    char     error_msg[512];
    bool     uses_karu;      /* any karu op present */
    bool     all_classical;  /* specialization result */
} IrModule;

/* Lower AST Program → IR */
IrModule *ir_compile(Program *prog);
void      ir_free(IrModule *m);

/* Emit debug dump */
void ir_disasm(const IrModule *m, FILE *out);

int ir_find_local(IrModule *m, const char *name);
int ir_add_local(IrModule *m, const char *name, QAType type);
int ir_intern_string(IrModule *m, const char *s);

#endif /* QUANTI_IR_H */
