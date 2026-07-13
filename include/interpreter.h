#ifndef QUANTI_INTERPRETER_H
#define QUANTI_INTERPRETER_H

#include "ast.h"
#include "runtime.h"
#include <stdbool.h>

/*
 * Interpreter — Tree-walking interpreter del lenguaje QA.
 *
 * Ejecuta un AST contra el QuantiRuntime.
 * Maneja scopes, variables, funciones y I/O.
 */

/* ── Valores del intérprete ─────────────────────────── */

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_KARU,
    VAL_VOID,
} ValueType;

typedef struct {
    ValueType type;
    union {
        int       int_val;
        double    float_val;
        char     *str_val;
        bool      bool_val;
        KaruByte  karu_val;
    } as;
} Value;

/* ── Scope (variables) ──────────────────────────────── */

typedef struct {
    char   *name;
    Value   value;
} Binding;

typedef struct Scope {
    Binding      *bindings;
    size_t        count;
    size_t        capacity;
    struct Scope *parent;
} Scope;

/* ── Función registrada ─────────────────────────────── */

typedef struct {
    char    *name;
    ASTNode *decl;   /* NODE_FN_DECL */
} FnEntry;

/* ── Intérprete ─────────────────────────────────────── */

typedef struct {
    QuantiRuntime *rt;
    Scope         *global_scope;
    FnEntry       *functions;
    size_t         fn_count;
    size_t         fn_cap;
    bool           had_error;
    char           error_msg[512];
    bool           returning;
    Value          return_value;
} Interpreter;

/* ── API ────────────────────────────────────────────── */

Interpreter *interp_create(QuantiConfig config);
void         interp_destroy(Interpreter *interp);

/* Ejecuta un programa completo */
bool interp_run(Interpreter *interp, Program *prog);

/* Ejecuta un string de código fuente directamente */
bool interp_exec(Interpreter *interp, const char *source);

#endif /* QUANTI_INTERPRETER_H */
