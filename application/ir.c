#include "ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static void ir_error(IrModule *m, int line, const char *msg) {
    if (m->had_error) return;
    m->had_error = true;
    snprintf(m->error_msg, sizeof(m->error_msg), "IR error (line %d): %s", line, msg);
}

static void ir_emit(IrModule *m, IrOpcode op, int32_t operand, int32_t aux,
                    double fimm, int line) {
    if (m->had_error) return;
    if (m->count >= m->capacity) {
        m->capacity = m->capacity ? m->capacity * 2 : 64;
        m->code = realloc(m->code, m->capacity * sizeof(IrInst));
    }
    IrInst *inst = &m->code[m->count++];
    inst->op = op;
    inst->operand = operand;
    inst->aux = aux;
    inst->fimm = fimm;
    inst->classical = false;
    inst->line = line;

    switch (op) {
    case IR_KARU_SUPER:
    case IR_KARU_AND:
    case IR_KARU_OR:
    case IR_KARU_NOT:
    case IR_MEASURE:
    case IR_P_NORMAL:
    case IR_P_UNIFORM:
    case IR_P_DISCRETE:
    case IR_FORK_IF:
        m->uses_karu = true;
        break;
    default:
        break;
    }
}

static void ir_patch(IrModule *m, size_t idx, int32_t operand) {
    if (idx < m->count) m->code[idx].operand = operand;
}

int ir_intern_string(IrModule *m, const char *s) {
    for (size_t i = 0; i < m->string_count; i++) {
        if (strcmp(m->strings[i], s) == 0) return (int)i;
    }
    if (m->string_count >= m->string_cap) {
        m->string_cap = m->string_cap ? m->string_cap * 2 : 8;
        m->strings = realloc(m->strings, m->string_cap * sizeof(char *));
    }
    m->strings[m->string_count] = strdup(s);
    return (int)m->string_count++;
}

int ir_find_local(IrModule *m, const char *name) {
    for (size_t i = 0; i < m->local_count; i++) {
        if (strcmp(m->locals[i].name, name) == 0)
            return m->locals[i].slot;
    }
    return -1;
}

int ir_add_local(IrModule *m, const char *name, QAType type) {
    int existing = ir_find_local(m, name);
    if (existing >= 0) return existing;

    if (m->local_count >= m->local_cap) {
        m->local_cap = m->local_cap ? m->local_cap * 2 : 16;
        m->locals = realloc(m->locals, m->local_cap * sizeof(IrLocal));
    }
    int slot = (int)m->local_count;
    m->locals[m->local_count].name = strdup(name);
    m->locals[m->local_count].type = type;
    m->locals[m->local_count].slot = slot;
    m->local_count++;
    return slot;
}

static int ir_find_func(IrModule *m, const char *name) {
    for (size_t i = 0; i < m->func_count; i++) {
        if (strcmp(m->funcs[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

static void register_functions(IrModule *m, Program *prog) {
    for (size_t i = 0; i < prog->count; i++) {
        ASTNode *node = prog->stmts[i];
        if (node->type != NODE_FN_DECL) continue;

        if (m->func_count >= m->func_cap) {
            m->func_cap = m->func_cap ? m->func_cap * 2 : 8;
            m->funcs = realloc(m->funcs, m->func_cap * sizeof(IrFunc));
        }
        IrFunc *fn = &m->funcs[m->func_count++];
        fn->name = strdup(node->as.fn_decl.fn_name);
        fn->entry_ip = -1;
        fn->arity = (int)node->as.fn_decl.fn_param_count;
        fn->ret_type = node->as.fn_decl.fn_return_type;
        fn->param_slots = malloc(fn->arity * sizeof(int));
        for (int p = 0; p < fn->arity; p++) {
            fn->param_slots[p] = ir_add_local(m, node->as.fn_decl.fn_params[p].name,
                                              node->as.fn_decl.fn_params[p].type);
        }
    }
}

static IrOpcode binop_to_ir(BinaryOp op) {
    switch (op) {
    case OP_ADD: return IR_ADD;
    case OP_SUB: return IR_SUB;
    case OP_MUL: return IR_MUL;
    case OP_DIV: return IR_DIV;
    case OP_EQ:  return IR_EQ;
    case OP_NEQ: return IR_NEQ;
    case OP_LT:  return IR_LT;
    case OP_GT:  return IR_GT;
    case OP_LTE: return IR_LTE;
    case OP_GTE: return IR_GTE;
    default:     return IR_NOP;
    }
}

static void compile_expr(IrModule *m, ASTNode *node);
static void compile_stmt(IrModule *m, ASTNode *node);

static void compile_block_stmts(IrModule *m, ASTNode *block) {
    if (!block) return;
    if (block->type != NODE_BLOCK) {
        compile_stmt(m, block);
        return;
    }
    for (size_t i = 0; i < block->as.block.block_count; i++)
        compile_stmt(m, block->as.block.block_stmts[i]);
}

static void compile_expr(IrModule *m, ASTNode *node) {
    if (!node || m->had_error) return;

    switch (node->type) {
    case NODE_INT_LIT:
        ir_emit(m, IR_CONST_I, node->as.int_lit.int_val, 0, 0.0, node->line);
        break;

    case NODE_FLOAT_LIT:
        ir_emit(m, IR_CONST_F, 0, 0, node->as.float_lit.float_val, node->line);
        break;

    case NODE_STRING_LIT: {
        int idx = ir_intern_string(m, node->as.string_lit.str_val);
        ir_emit(m, IR_CONST_S, idx, 0, 0.0, node->line);
        break;
    }

    case NODE_BOOL_LIT:
        ir_emit(m, IR_CONST_B, node->as.bool_lit.bool_val ? 1 : 0, 0, 0.0, node->line);
        break;

    case NODE_IDENT: {
        int slot = ir_find_local(m, node->as.ident.ident_name);
        if (slot < 0) {
            ir_error(m, node->line, "Undefined variable");
            return;
        }
        ir_emit(m, IR_LOAD, slot, 0, 0.0, node->line);
        break;
    }

    case NODE_BINARY:
        compile_expr(m, node->as.binary.left);
        compile_expr(m, node->as.binary.right);
        ir_emit(m, binop_to_ir(node->as.binary.op), 0, 0, 0.0, node->line);
        break;

    case NODE_UNARY:
        compile_expr(m, node->as.unary.operand);
        ir_emit(m, IR_NEG, 0, 0, 0.0, node->line);
        break;

    case NODE_KARU_AND:
        compile_expr(m, node->as.binary.left);
        compile_expr(m, node->as.binary.right);
        ir_emit(m, IR_KARU_AND, 0, 0, 0.0, node->line);
        break;

    case NODE_KARU_OR:
        compile_expr(m, node->as.binary.left);
        compile_expr(m, node->as.binary.right);
        ir_emit(m, IR_KARU_OR, 0, 0, 0.0, node->line);
        break;

    case NODE_KARU_NOT:
        compile_expr(m, node->as.unary.operand);
        ir_emit(m, IR_KARU_NOT, 0, 0, 0.0, node->line);
        break;

    case NODE_SUPERPOSITION: {
        size_t n = node->as.superposition.super_count;
        if (n != 2) {
            for (size_t i = 0; i < n; i++)
                compile_expr(m, node->as.superposition.super_args[i]);
        }
        ir_emit(m, IR_KARU_SUPER, 0, (int32_t)n, 0.0, node->line);
        if (n != 2 && n > 0) {
            for (size_t i = 0; i < n; i++)
                ir_emit(m, IR_POP, 0, 0, 0.0, node->line);
        }
        break;
    }

    case NODE_MEASURE:
        compile_expr(m, node->as.measure.measure_expr);
        ir_emit(m, IR_MEASURE, (int32_t)node->as.measure.measure_mode, 0, 0.0, node->line);
        break;

    case NODE_P_DIST:
        if (node->as.p_dist.dist_type == DIST_AST_NORMAL &&
            node->as.p_dist.dist_arg_count == 2) {
            compile_expr(m, node->as.p_dist.dist_args[0]);
            compile_expr(m, node->as.p_dist.dist_args[1]);
            ir_emit(m, IR_P_NORMAL, 0, 0, 0.0, node->line);
        } else if (node->as.p_dist.dist_type == DIST_AST_UNIFORM &&
                   node->as.p_dist.dist_arg_count == 2) {
            compile_expr(m, node->as.p_dist.dist_args[0]);
            compile_expr(m, node->as.p_dist.dist_args[1]);
            ir_emit(m, IR_P_UNIFORM, 0, 0, 0.0, node->line);
        } else if (node->as.p_dist.dist_type == DIST_AST_DISCRETE &&
                   node->as.p_dist.dist_arg_count >= 1) {
            ASTNode *arr = node->as.p_dist.dist_args[0];
            if (arr->type != NODE_ARRAY_LIT) {
                ir_error(m, node->line, "Discrete P requires literal float array");
                return;
            }
            size_t n = arr->as.array_lit.array_count;
            int label_start = -1;
            if (node->as.p_dist.dist_arg_count >= 2) {
                ASTNode *labels = node->as.p_dist.dist_args[1];
                if (labels->type == NODE_ARRAY_LIT && labels->as.array_lit.array_count > 0) {
                    label_start = ir_intern_string(m,
                        labels->as.array_lit.array_elems[0]->as.string_lit.str_val);
                    for (size_t li = 1; li < n && li < labels->as.array_lit.array_count; li++) {
                        ir_intern_string(m,
                            labels->as.array_lit.array_elems[li]->as.string_lit.str_val);
                    }
                }
            }
            for (size_t i = 0; i < n; i++) {
                ASTNode *el = arr->as.array_lit.array_elems[i];
                if (el->type == NODE_FLOAT_LIT)
                    ir_emit(m, IR_CONST_F, 0, 0, el->as.float_lit.float_val, node->line);
                else if (el->type == NODE_INT_LIT)
                    ir_emit(m, IR_CONST_F, 0, 0, (double)el->as.int_lit.int_val, node->line);
                else {
                    ir_error(m, node->line, "Discrete probs must be numeric literals");
                    return;
                }
            }
            ir_emit(m, IR_P_DISCRETE, (int32_t)n, label_start, 0.0, node->line);
        } else {
            ir_error(m, node->line, "Invalid distribution");
        }
        break;

    case NODE_CALL: {
        int fi = ir_find_func(m, node->as.call.call_name);
        if (fi < 0) {
            ir_error(m, node->line, "Undefined function");
            return;
        }
        if ((int)node->as.call.call_arg_count != m->funcs[fi].arity) {
            ir_error(m, node->line, "Wrong number of arguments");
            return;
        }
        for (size_t i = 0; i < node->as.call.call_arg_count; i++)
            compile_expr(m, node->as.call.call_args[i]);
        ir_emit(m, IR_CALL, fi, (int32_t)node->as.call.call_arg_count, 0.0, node->line);
        break;
    }

    default:
        ir_error(m, node->line, "Unsupported expression in IR");
        break;
    }
}

static void compile_stmt(IrModule *m, ASTNode *node) {
    if (!node || m->had_error) return;

    switch (node->type) {
    case NODE_VAR_DECL: {
        int slot = ir_add_local(m, node->as.var_decl.var_name, node->as.var_decl.var_type);
        if (node->as.var_decl.var_init) {
            compile_expr(m, node->as.var_decl.var_init);
            ir_emit(m, IR_STORE, slot, 0, 0.0, node->line);
        }
        break;
    }

    case NODE_ASSIGN: {
        int slot = ir_find_local(m, node->as.assign.assign_name);
        if (slot < 0) {
            ir_error(m, node->line, "Undefined variable in assignment");
            return;
        }
        compile_expr(m, node->as.assign.assign_value);
        ir_emit(m, IR_STORE, slot, 0, 0.0, node->line);
        break;
    }

    case NODE_PRINT:
        compile_expr(m, node->as.print.print_expr);
        ir_emit(m, IR_PRINT, 0, 0, 0.0, node->line);
        break;

    case NODE_IF: {
        compile_expr(m, node->as.if_stmt.if_cond);
        size_t jmp_else = m->count;
        ir_emit(m, IR_JMP_IF_NOT, 0, 0, 0.0, node->line);
        compile_block_stmts(m, node->as.if_stmt.if_then);
        size_t jmp_end = m->count;
        ir_emit(m, IR_JMP, 0, 0, 0.0, node->line);
        ir_patch(m, jmp_else, (int32_t)m->count);
        if (node->as.if_stmt.if_else)
            compile_block_stmts(m, node->as.if_stmt.if_else);
        ir_patch(m, jmp_end, (int32_t)m->count);
        break;
    }

    case NODE_WHEN: {
        compile_expr(m, node->as.when_stmt.when_cond);
        size_t jmp_else = m->count;
        ir_emit(m, IR_JMP_IF_NOT, 0, 0, 0.0, node->line);
        compile_block_stmts(m, node->as.when_stmt.when_body);
        ir_patch(m, jmp_else, (int32_t)m->count);
        break;
    }

    case NODE_WHILE: {
        size_t loop_start = m->count;
        compile_expr(m, node->as.while_stmt.while_cond);
        size_t jmp_out = m->count;
        ir_emit(m, IR_JMP_IF_NOT, 0, 0, 0.0, node->line);
        compile_block_stmts(m, node->as.while_stmt.while_body);
        ir_emit(m, IR_JMP, (int32_t)loop_start, 0, 0.0, node->line);
        ir_patch(m, jmp_out, (int32_t)m->count);
        break;
    }

    case NODE_BLOCK:
        compile_block_stmts(m, node);
        break;

    case NODE_FN_DECL:
        break;

    case NODE_RETURN:
        if (node->as.ret.ret_expr)
            compile_expr(m, node->as.ret.ret_expr);
        ir_emit(m, IR_RET, 0, 0, 0.0, node->line);
        break;

    case NODE_EXPR_STMT:
        compile_expr(m, node->as.expr_stmt.expr);
        ir_emit(m, IR_POP, 0, 0, 0.0, node->line);
        break;

    case NODE_RUNTIME_CFG: {
        int32_t flags = 0;
        if (node->as.runtime_cfg.has_max_branches) flags |= 1;
        if (node->as.runtime_cfg.has_prune_threshold) flags |= 2;
        ir_emit(m, IR_RUNTIME_CFG,
                (int32_t)node->as.runtime_cfg.max_branches,
                flags,
                node->as.runtime_cfg.prune_threshold,
                node->line);
        break;
    }

    default:
        ir_error(m, node->line, "Unsupported statement in IR");
        break;
    }
}

static void emit_function_bodies(IrModule *m, Program *prog) {
    size_t fi = 0;
    for (size_t i = 0; i < prog->count; i++) {
        ASTNode *node = prog->stmts[i];
        if (node->type != NODE_FN_DECL) continue;
        if (fi >= m->func_count) break;
        m->funcs[fi].entry_ip = (int)m->count;
        compile_block_stmts(m, node->as.fn_decl.fn_body);
        fi++;
    }
}

IrModule *ir_compile(Program *prog) {
    IrModule *m = calloc(1, sizeof(IrModule));
    if (!m) return NULL;

    register_functions(m, prog);

    for (size_t i = 0; i < prog->count; i++) {
        ASTNode *node = prog->stmts[i];
        if (node->type == NODE_FN_DECL) continue;
        compile_stmt(m, node);
    }

    emit_function_bodies(m, prog);
    ir_emit(m, IR_HALT, 0, 0, 0.0, 0);

    return m;
}

void ir_free(IrModule *m) {
    if (!m) return;
    free(m->code);
    for (size_t i = 0; i < m->local_count; i++)
        free(m->locals[i].name);
    free(m->locals);
    for (size_t i = 0; i < m->func_count; i++) {
        free(m->funcs[i].name);
        free(m->funcs[i].param_slots);
    }
    free(m->funcs);
    for (size_t i = 0; i < m->string_count; i++)
        free(m->strings[i]);
    free(m->strings);
    free(m);
}

static const char *op_name(IrOpcode op) {
    switch (op) {
    case IR_NOP: return "NOP";
    case IR_CONST_I: return "CONST_I";
    case IR_CONST_F: return "CONST_F";
    case IR_CONST_S: return "CONST_S";
    case IR_CONST_B: return "CONST_B";
    case IR_LOAD: return "LOAD";
    case IR_STORE: return "STORE";
    case IR_ADD: return "ADD";
    case IR_SUB: return "SUB";
    case IR_MUL: return "MUL";
    case IR_DIV: return "DIV";
    case IR_EQ: return "EQ";
    case IR_NEQ: return "NEQ";
    case IR_LT: return "LT";
    case IR_GT: return "GT";
    case IR_LTE: return "LTE";
    case IR_GTE: return "GTE";
    case IR_NEG: return "NEG";
    case IR_JMP: return "JMP";
    case IR_JMP_IF: return "JMP_IF";
    case IR_JMP_IF_NOT: return "JMP_IF_NOT";
    case IR_PRINT: return "PRINT";
    case IR_POP: return "POP";
    case IR_CALL: return "CALL";
    case IR_RET: return "RET";
    case IR_KARU_SUPER: return "KARU_SUPER";
    case IR_KARU_AND: return "KARU_AND";
    case IR_KARU_OR: return "KARU_OR";
    case IR_KARU_NOT: return "KARU_NOT";
    case IR_MEASURE: return "MEASURE";
    case IR_P_NORMAL: return "P_NORMAL";
    case IR_P_UNIFORM: return "P_UNIFORM";
    case IR_P_DISCRETE: return "P_DISCRETE";
    case IR_FORK_IF: return "FORK_IF";
    case IR_RUNTIME_CFG: return "RUNTIME_CFG";
    case IR_HALT: return "HALT";
    default: return "?";
    }
}

void ir_disasm(const IrModule *m, FILE *out) {
    if (!m || !out) return;
    fprintf(out, ";; IR module: %zu insts, %zu locals, %zu funcs\n",
            m->count, m->local_count, m->func_count);
    for (size_t i = 0; i < m->count; i++) {
        const IrInst *in = &m->code[i];
        fprintf(out, "%4zu  %-14s op=%d aux=%d", i, op_name(in->op), in->operand, in->aux);
        if (in->op == IR_CONST_F || in->op == IR_RUNTIME_CFG)
            fprintf(out, " f=%g", in->fimm);
        fprintf(out, "\n");
    }
}
