#include "typecheck.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char   *name;
    QAType  type;
} Sym;

typedef struct Scope {
    Sym            *syms;
    size_t          count;
    size_t          cap;
    struct Scope   *parent;
} Scope;

static Scope *scope_push(Scope *parent) {
    Scope *s = calloc(1, sizeof(Scope));
    s->cap = 16;
    s->syms = calloc(s->cap, sizeof(Sym));
    s->parent = parent;
    return s;
}

static void scope_pop(Scope *s) {
    if (!s) return;
    for (size_t i = 0; i < s->count; i++)
        free(s->syms[i].name);
    free(s->syms);
    free(s);
}

static void scope_bind(Scope *s, const char *name, QAType type) {
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->syms[i].name, name) == 0) {
            s->syms[i].type = type;
            return;
        }
    }
    if (s->count >= s->cap) {
        s->cap *= 2;
        s->syms = realloc(s->syms, s->cap * sizeof(Sym));
    }
    s->syms[s->count].name = strdup(name);
    s->syms[s->count].type = type;
    s->count++;
}

static Sym *scope_lookup(Scope *s, const char *name) {
    for (Scope *cur = s; cur; cur = cur->parent) {
        for (size_t i = 0; i < cur->count; i++) {
            if (strcmp(cur->syms[i].name, name) == 0)
                return &cur->syms[i];
        }
    }
    return NULL;
}

typedef struct {
    TypeCheckResult result;
    Scope          *scope;
    ASTNode       **fn_decls;
    size_t          fn_count;
    size_t          fn_cap;
} Ctx;

static void tc_error(Ctx *ctx, int line, const char *msg) {
    if (ctx->result.had_error) return;
    ctx->result.had_error = true;
    snprintf(ctx->result.error_msg, sizeof(ctx->result.error_msg),
             "Type error (line %d): %s", line, msg);
}

static bool is_karuish(QAType t) {
    return t == QA_TYPE_KARU || t == QA_TYPE_INT || t == QA_TYPE_BOOL;
}

static QAType tc_expr(Ctx *ctx, ASTNode *node);

static QAType tc_binary(Ctx *ctx, ASTNode *node) {
    QAType lt = tc_expr(ctx, node->as.binary.left);
    QAType rt = tc_expr(ctx, node->as.binary.right);
    if (lt == QA_TYPE_VOID || rt == QA_TYPE_VOID)
        return QA_TYPE_VOID;
    if (lt == QA_TYPE_INT && rt == QA_TYPE_INT) return QA_TYPE_INT;
    if (lt == QA_TYPE_FLOAT || rt == QA_TYPE_FLOAT) return QA_TYPE_FLOAT;
    if (lt == QA_TYPE_STRING && rt == QA_TYPE_STRING &&
        (node->as.binary.op == OP_EQ || node->as.binary.op == OP_NEQ))
        return QA_TYPE_BOOL;
    tc_error(ctx, node->line, "Type mismatch in binary op");
    return QA_TYPE_VOID;
}

static QAType tc_expr(Ctx *ctx, ASTNode *node) {
    if (!node || ctx->result.had_error) return QA_TYPE_VOID;

    switch (node->type) {
    case NODE_INT_LIT:    return QA_TYPE_INT;
    case NODE_FLOAT_LIT:  return QA_TYPE_FLOAT;
    case NODE_STRING_LIT: return QA_TYPE_STRING;
    case NODE_BOOL_LIT:   return QA_TYPE_BOOL;

    case NODE_IDENT: {
        Sym *s = scope_lookup(ctx->scope, node->as.ident.ident_name);
        if (!s) {
            tc_error(ctx, node->line, "Undefined variable");
            return QA_TYPE_VOID;
        }
        return s->type;
    }

    case NODE_BINARY:
        return tc_binary(ctx, node);

    case NODE_UNARY:
        if (tc_expr(ctx, node->as.unary.operand) == QA_TYPE_INT)
            return QA_TYPE_INT;
        tc_error(ctx, node->line, "Unary - requires int");
        return QA_TYPE_VOID;

    case NODE_KARU_AND:
    case NODE_KARU_OR: {
        QAType l = tc_expr(ctx, node->as.binary.left);
        QAType r = tc_expr(ctx, node->as.binary.right);
        if (!is_karuish(l) || !is_karuish(r))
            tc_error(ctx, node->line, "AND/OR requires karu or int");
        return QA_TYPE_KARU;
    }

    case NODE_KARU_NOT: {
        QAType o = tc_expr(ctx, node->as.unary.operand);
        if (!is_karuish(o))
            tc_error(ctx, node->line, "NOT requires karu or int");
        return QA_TYPE_KARU;
    }

    case NODE_SUPERPOSITION:
        return QA_TYPE_KARU;

    case NODE_MEASURE: {
        QAType e = tc_expr(ctx, node->as.measure.measure_expr);
        if (e != QA_TYPE_KARU)
            tc_error(ctx, node->line, "measure requires karu value");
        return QA_TYPE_INT;
    }

    case NODE_P_DIST:
        return QA_TYPE_KARU;

    case NODE_CALL: {
        ASTNode *decl = NULL;
        for (size_t i = 0; i < ctx->fn_count; i++) {
            if (strcmp(ctx->fn_decls[i]->as.fn_decl.fn_name, node->as.call.call_name) == 0) {
                decl = ctx->fn_decls[i];
                break;
            }
        }
        if (!decl) {
            tc_error(ctx, node->line, "Undefined function");
            return QA_TYPE_VOID;
        }
        if (node->as.call.call_arg_count != decl->as.fn_decl.fn_param_count) {
            tc_error(ctx, node->line, "Wrong number of arguments");
            return QA_TYPE_VOID;
        }
        for (size_t i = 0; i < node->as.call.call_arg_count; i++)
            tc_expr(ctx, node->as.call.call_args[i]);
        return decl->as.fn_decl.fn_return_type;
    }

    default:
        tc_error(ctx, node->line, "Invalid expression");
        return QA_TYPE_VOID;
    }
}

static void tc_block(Ctx *ctx, ASTNode *block);

static void tc_stmt(Ctx *ctx, ASTNode *node) {
    if (!node || ctx->result.had_error) return;

    switch (node->type) {
    case NODE_VAR_DECL: {
        if (node->as.var_decl.var_init)
            tc_expr(ctx, node->as.var_decl.var_init);
        scope_bind(ctx->scope, node->as.var_decl.var_name, node->as.var_decl.var_type);
        break;
    }

    case NODE_ASSIGN: {
        Sym *s = scope_lookup(ctx->scope, node->as.assign.assign_name);
        if (!s) {
            tc_error(ctx, node->line, "Undefined variable in assignment");
            return;
        }
        tc_expr(ctx, node->as.assign.assign_value);
        break;
    }

    case NODE_PRINT:
        tc_expr(ctx, node->as.print.print_expr);
        break;

    case NODE_IF:
        tc_expr(ctx, node->as.if_stmt.if_cond);
        tc_block(ctx, node->as.if_stmt.if_then);
        if (node->as.if_stmt.if_else)
            tc_block(ctx, node->as.if_stmt.if_else);
        break;

    case NODE_WHEN:
        tc_expr(ctx, node->as.when_stmt.when_cond);
        tc_block(ctx, node->as.when_stmt.when_body);
        break;

    case NODE_WHILE:
        tc_expr(ctx, node->as.while_stmt.while_cond);
        tc_block(ctx, node->as.while_stmt.while_body);
        break;

    case NODE_BLOCK:
        tc_block(ctx, node);
        break;

    case NODE_FN_DECL: {
        if (ctx->fn_count >= ctx->fn_cap) {
            ctx->fn_cap *= 2;
            ctx->fn_decls = realloc(ctx->fn_decls, ctx->fn_cap * sizeof(ASTNode *));
        }
        ctx->fn_decls[ctx->fn_count++] = node;

        Scope *fn_scope = scope_push(ctx->scope);
        for (size_t i = 0; i < node->as.fn_decl.fn_param_count; i++)
            scope_bind(fn_scope, node->as.fn_decl.fn_params[i].name,
                       node->as.fn_decl.fn_params[i].type);
        ctx->scope = fn_scope;
        tc_block(ctx, node->as.fn_decl.fn_body);
        ctx->scope = fn_scope->parent;
        scope_pop(fn_scope);
        break;
    }

    case NODE_RETURN:
        if (node->as.ret.ret_expr)
            tc_expr(ctx, node->as.ret.ret_expr);
        break;

    case NODE_EXPR_STMT:
        tc_expr(ctx, node->as.expr_stmt.expr);
        break;

    case NODE_RUNTIME_CFG:
        break;

    default:
        tc_error(ctx, node->line, "Unsupported statement");
        break;
    }
}

static void tc_block(Ctx *ctx, ASTNode *block) {
    if (!block) return;
    if (block->type != NODE_BLOCK) {
        tc_stmt(ctx, block);
        return;
    }
    Scope *inner = scope_push(ctx->scope);
    ctx->scope = inner;
    for (size_t i = 0; i < block->as.block.block_count; i++)
        tc_stmt(ctx, block->as.block.block_stmts[i]);
    ctx->scope = inner->parent;
    scope_pop(inner);
}

TypeCheckResult typecheck_program(Program *prog) {
    Ctx ctx = {0};
    ctx.scope = scope_push(NULL);
    ctx.fn_cap = 8;
    ctx.fn_decls = malloc(ctx.fn_cap * sizeof(ASTNode *));

    for (size_t i = 0; i < prog->count; i++)
        tc_stmt(&ctx, prog->stmts[i]);

    free(ctx.fn_decls);
    scope_pop(ctx.scope);
    return ctx.result;
}
