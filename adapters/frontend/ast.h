#ifndef QUANTI_AST_H
#define QUANTI_AST_H

#include <stddef.h>
#include <stdbool.h>

/*
 * AST — Árbol Sintáctico Abstracto del lenguaje QA.
 *
 * Nodos para: declaraciones, expresiones, statements,
 * operaciones KaruByte, measure, superposition, funciones.
 */

typedef enum {
    /* ── Statements ─────────────── */
    NODE_VAR_DECL,       /* int x = 5;  /  karu k = superposition(0,1); */
    NODE_ASSIGN,         /* x = expr; */
    NODE_PRINT,          /* print(expr); */
    NODE_IF,             /* if (cond) { ... } else { ... } */
    NODE_WHILE,          /* while (cond) { ... } */
    NODE_BLOCK,          /* { stmt; stmt; ... } */
    NODE_FN_DECL,        /* fn name(params) -> type { body } */
    NODE_RETURN,         /* return expr; */
    NODE_EXPR_STMT,      /* expression as statement (function call) */
    NODE_WHEN,           /* when (cond) { ... } */
    NODE_RUNTIME_CFG,    /* @runtime(max_branches: N, prune_threshold: F) */

    /* ── Expressions ────────────── */
    NODE_INT_LIT,        /* 42 */
    NODE_FLOAT_LIT,      /* 3.14 */
    NODE_STRING_LIT,     /* "hello" */
    NODE_BOOL_LIT,       /* true / false */
    NODE_IDENT,          /* variable name */
    NODE_BINARY,         /* a OP b  (+, -, *, /, ==, !=, <, >, <=, >=) */
    NODE_UNARY,          /* -x, NOT x */
    NODE_CALL,           /* fn_name(args) */
    NODE_KARU_AND,       /* x AND y */
    NODE_KARU_OR,        /* x OR y */
    NODE_KARU_NOT,       /* NOT x */
    NODE_SUPERPOSITION,  /* superposition(a, b, c) */
    NODE_MEASURE,        /* measure:map(x) / measure:sample(x) / measure:first(x) */
    NODE_P_DIST,         /* P(Discrete(...)) / P(Normal(...)) / P(Uniform(...)) */
    NODE_ARRAY_LIT,      /* [a, b, c] */
} NodeType;

/* ── Tipos del lenguaje QA ──────────────────────────── */

typedef enum {
    QA_TYPE_INT,
    QA_TYPE_FLOAT,
    QA_TYPE_STRING,
    QA_TYPE_BOOL,
    QA_TYPE_KARU,
    QA_TYPE_VOID,
    QA_TYPE_INFER,      /* type inference */
} QAType;

/* ── Operadores binarios ────────────────────────────── */

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LTE, OP_GTE,
} BinaryOp;

/* ── Modos de measure ───────────────────────────────── */

typedef enum {
    MEASURE_MAP,
    MEASURE_SAMPLE,
    MEASURE_FIRST,
} MeasureMode;

/* ── Tipos de distribución ──────────────────────────── */

typedef enum {
    DIST_AST_NORMAL,
    DIST_AST_DISCRETE,
    DIST_AST_UNIFORM,
} DistAstType;

/* ── ASTNode ────────────────────────────────────────── */

typedef struct ASTNode ASTNode;

typedef struct {
    char    *name;
    QAType   type;
} Param;

struct ASTNode {
    NodeType type;
    int      line;

    union {
        /* NODE_VAR_DECL */
        struct {
            QAType    var_type;
            char     *var_name;
            ASTNode  *var_init;      /* nullable */
            bool      var_persistent;
        } var_decl;

        /* NODE_ASSIGN */
        struct {
            char    *assign_name;
            ASTNode *assign_value;
        } assign;

        /* NODE_PRINT */
        struct { ASTNode *print_expr; } print;

        /* NODE_IF */
        struct {
            ASTNode *if_cond;
            ASTNode *if_then;
            ASTNode *if_else;   /* nullable */
        } if_stmt;

        /* NODE_WHEN */
        struct {
            ASTNode *when_cond;
            ASTNode *when_body;
        } when_stmt;

        /* NODE_RUNTIME_CFG */
        struct {
            size_t max_branches;     /* 0 = unchanged */
            double prune_threshold;  /* < 0 = unchanged */
            bool   has_max_branches;
            bool   has_prune_threshold;
        } runtime_cfg;

        /* NODE_WHILE */
        struct {
            ASTNode *while_cond;
            ASTNode *while_body;
        } while_stmt;

        /* NODE_BLOCK */
        struct {
            ASTNode **block_stmts;
            size_t    block_count;
        } block;

        /* NODE_FN_DECL */
        struct {
            char    *fn_name;
            Param   *fn_params;
            size_t   fn_param_count;
            QAType   fn_return_type;
            ASTNode *fn_body;
        } fn_decl;

        /* NODE_RETURN */
        struct { ASTNode *ret_expr; } ret;

        /* NODE_EXPR_STMT */
        struct { ASTNode *expr; } expr_stmt;

        /* NODE_INT_LIT */
        struct { int int_val; } int_lit;

        /* NODE_FLOAT_LIT */
        struct { double float_val; } float_lit;

        /* NODE_STRING_LIT */
        struct { char *str_val; } string_lit;

        /* NODE_BOOL_LIT */
        struct { bool bool_val; } bool_lit;

        /* NODE_IDENT */
        struct { char *ident_name; } ident;

        /* NODE_BINARY, NODE_KARU_AND, NODE_KARU_OR */
        struct {
            BinaryOp  op;
            ASTNode  *left;
            ASTNode  *right;
        } binary;

        /* NODE_UNARY, NODE_KARU_NOT */
        struct { ASTNode *operand; } unary;

        /* NODE_CALL */
        struct {
            char     *call_name;
            ASTNode **call_args;
            size_t    call_arg_count;
        } call;

        /* NODE_SUPERPOSITION */
        struct {
            ASTNode **super_args;
            size_t    super_count;
        } superposition;

        /* NODE_MEASURE */
        struct {
            MeasureMode measure_mode;
            ASTNode    *measure_expr;
        } measure;

        /* NODE_P_DIST */
        struct {
            DistAstType dist_type;
            ASTNode   **dist_args;
            size_t      dist_arg_count;
        } p_dist;

        /* NODE_ARRAY_LIT */
        struct {
            ASTNode **array_elems;
            size_t    array_count;
        } array_lit;
    } as;
};

/* ── Programa completo ──────────────────────────────── */

typedef struct {
    ASTNode **stmts;
    size_t    count;
} Program;

/* ── Ciclo de vida ──────────────────────────────────── */

ASTNode *ast_alloc(NodeType type, int line);
void     ast_free(ASTNode *node);
void     program_free(Program *prog);

/* ── Debug ──────────────────────────────────────────── */

void ast_print(ASTNode *node, int indent);

#endif /* QUANTI_AST_H */
