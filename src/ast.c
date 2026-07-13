#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

ASTNode *ast_alloc(NodeType type, int line) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    node->line = line;
    return node;
}

void ast_free(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
    case NODE_VAR_DECL:
        free(node->as.var_decl.var_name);
        ast_free(node->as.var_decl.var_init);
        break;
    case NODE_ASSIGN:
        free(node->as.assign.assign_name);
        ast_free(node->as.assign.assign_value);
        break;
    case NODE_PRINT:
        ast_free(node->as.print.print_expr);
        break;
    case NODE_IF:
        ast_free(node->as.if_stmt.if_cond);
        ast_free(node->as.if_stmt.if_then);
        ast_free(node->as.if_stmt.if_else);
        break;
    case NODE_WHILE:
        ast_free(node->as.while_stmt.while_cond);
        ast_free(node->as.while_stmt.while_body);
        break;
    case NODE_BLOCK:
        for (size_t i = 0; i < node->as.block.block_count; i++)
            ast_free(node->as.block.block_stmts[i]);
        free(node->as.block.block_stmts);
        break;
    case NODE_FN_DECL:
        free(node->as.fn_decl.fn_name);
        for (size_t i = 0; i < node->as.fn_decl.fn_param_count; i++)
            free(node->as.fn_decl.fn_params[i].name);
        free(node->as.fn_decl.fn_params);
        ast_free(node->as.fn_decl.fn_body);
        break;
    case NODE_RETURN:
        ast_free(node->as.ret.ret_expr);
        break;
    case NODE_EXPR_STMT:
        ast_free(node->as.expr_stmt.expr);
        break;
    case NODE_STRING_LIT:
        free(node->as.string_lit.str_val);
        break;
    case NODE_IDENT:
        free(node->as.ident.ident_name);
        break;
    case NODE_BINARY:
    case NODE_KARU_AND:
    case NODE_KARU_OR:
        ast_free(node->as.binary.left);
        ast_free(node->as.binary.right);
        break;
    case NODE_UNARY:
    case NODE_KARU_NOT:
        ast_free(node->as.unary.operand);
        break;
    case NODE_CALL:
        free(node->as.call.call_name);
        for (size_t i = 0; i < node->as.call.call_arg_count; i++)
            ast_free(node->as.call.call_args[i]);
        free(node->as.call.call_args);
        break;
    case NODE_SUPERPOSITION:
        for (size_t i = 0; i < node->as.superposition.super_count; i++)
            ast_free(node->as.superposition.super_args[i]);
        free(node->as.superposition.super_args);
        break;
    case NODE_MEASURE:
        ast_free(node->as.measure.measure_expr);
        break;
    case NODE_P_DIST:
        for (size_t i = 0; i < node->as.p_dist.dist_arg_count; i++)
            ast_free(node->as.p_dist.dist_args[i]);
        free(node->as.p_dist.dist_args);
        break;
    case NODE_ARRAY_LIT:
        for (size_t i = 0; i < node->as.array_lit.array_count; i++)
            ast_free(node->as.array_lit.array_elems[i]);
        free(node->as.array_lit.array_elems);
        break;
    default:
        break;
    }
    free(node);
}

void program_free(Program *prog) {
    if (!prog) return;
    for (size_t i = 0; i < prog->count; i++)
        ast_free(prog->stmts[i]);
    free(prog->stmts);
}

static void indent_print(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

void ast_print(ASTNode *node, int indent) {
    if (!node) { indent_print(indent); printf("(null)\n"); return; }

    indent_print(indent);
    switch (node->type) {
    case NODE_VAR_DECL:
        printf("VarDecl(%s, type=%d)\n", node->as.var_decl.var_name, node->as.var_decl.var_type);
        if (node->as.var_decl.var_init) ast_print(node->as.var_decl.var_init, indent+1);
        break;
    case NODE_ASSIGN:
        printf("Assign(%s)\n", node->as.assign.assign_name);
        ast_print(node->as.assign.assign_value, indent+1);
        break;
    case NODE_PRINT:
        printf("Print\n");
        ast_print(node->as.print.print_expr, indent+1);
        break;
    case NODE_IF:
        printf("If\n");
        ast_print(node->as.if_stmt.if_cond, indent+1);
        ast_print(node->as.if_stmt.if_then, indent+1);
        if (node->as.if_stmt.if_else) ast_print(node->as.if_stmt.if_else, indent+1);
        break;
    case NODE_WHILE:
        printf("While\n");
        ast_print(node->as.while_stmt.while_cond, indent+1);
        ast_print(node->as.while_stmt.while_body, indent+1);
        break;
    case NODE_BLOCK:
        printf("Block(%zu stmts)\n", node->as.block.block_count);
        for (size_t i = 0; i < node->as.block.block_count; i++)
            ast_print(node->as.block.block_stmts[i], indent+1);
        break;
    case NODE_FN_DECL:
        printf("FnDecl(%s)\n", node->as.fn_decl.fn_name);
        ast_print(node->as.fn_decl.fn_body, indent+1);
        break;
    case NODE_RETURN:
        printf("Return\n");
        ast_print(node->as.ret.ret_expr, indent+1);
        break;
    case NODE_INT_LIT:
        printf("IntLit(%d)\n", node->as.int_lit.int_val);
        break;
    case NODE_FLOAT_LIT:
        printf("FloatLit(%f)\n", node->as.float_lit.float_val);
        break;
    case NODE_STRING_LIT:
        printf("StringLit(\"%s\")\n", node->as.string_lit.str_val);
        break;
    case NODE_BOOL_LIT:
        printf("BoolLit(%s)\n", node->as.bool_lit.bool_val ? "true" : "false");
        break;
    case NODE_IDENT:
        printf("Ident(%s)\n", node->as.ident.ident_name);
        break;
    case NODE_BINARY:
        printf("Binary(op=%d)\n", node->as.binary.op);
        ast_print(node->as.binary.left, indent+1);
        ast_print(node->as.binary.right, indent+1);
        break;
    case NODE_KARU_AND:
        printf("KaruAND\n");
        ast_print(node->as.binary.left, indent+1);
        ast_print(node->as.binary.right, indent+1);
        break;
    case NODE_KARU_OR:
        printf("KaruOR\n");
        ast_print(node->as.binary.left, indent+1);
        ast_print(node->as.binary.right, indent+1);
        break;
    case NODE_KARU_NOT:
        printf("KaruNOT\n");
        ast_print(node->as.unary.operand, indent+1);
        break;
    case NODE_UNARY:
        printf("Unary\n");
        ast_print(node->as.unary.operand, indent+1);
        break;
    case NODE_CALL:
        printf("Call(%s, %zu args)\n", node->as.call.call_name, node->as.call.call_arg_count);
        break;
    case NODE_SUPERPOSITION:
        printf("Superposition(%zu values)\n", node->as.superposition.super_count);
        for (size_t i = 0; i < node->as.superposition.super_count; i++)
            ast_print(node->as.superposition.super_args[i], indent+1);
        break;
    case NODE_MEASURE:
        printf("Measure(mode=%d)\n", node->as.measure.measure_mode);
        ast_print(node->as.measure.measure_expr, indent+1);
        break;
    case NODE_P_DIST:
        printf("PDist(type=%d)\n", node->as.p_dist.dist_type);
        break;
    case NODE_ARRAY_LIT:
        printf("Array(%zu elems)\n", node->as.array_lit.array_count);
        break;
    case NODE_EXPR_STMT:
        printf("ExprStmt\n");
        ast_print(node->as.expr_stmt.expr, indent+1);
        break;
    }
}
