#include "interpreter.h"
#include "parser.h"
#include "distribution.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── Scope management ───────────────────────────────── */

static Scope *scope_create(Scope *parent) {
    Scope *s = calloc(1, sizeof(Scope));
    s->capacity = 16;
    s->bindings = calloc(s->capacity, sizeof(Binding));
    s->parent = parent;
    return s;
}

static void scope_free(Scope *s) {
    if (!s) return;
    for (size_t i = 0; i < s->count; i++) {
        free(s->bindings[i].name);
        if (s->bindings[i].value.type == VAL_STRING)
            free(s->bindings[i].value.as.str_val);
        if (s->bindings[i].value.type == VAL_KARU)
            karu_free(&s->bindings[i].value.as.karu_val);
    }
    free(s->bindings);
    free(s);
}

static void scope_set(Scope *s, const char *name, Value val) {
    /* Check if exists in this scope */
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->bindings[i].name, name) == 0) {
            if (s->bindings[i].value.type == VAL_STRING)
                free(s->bindings[i].value.as.str_val);
            if (s->bindings[i].value.type == VAL_KARU)
                karu_free(&s->bindings[i].value.as.karu_val);
            s->bindings[i].value = val;
            return;
        }
    }
    /* New binding */
    if (s->count >= s->capacity) {
        s->capacity *= 2;
        s->bindings = realloc(s->bindings, s->capacity * sizeof(Binding));
    }
    s->bindings[s->count].name = strdup(name);
    s->bindings[s->count].value = val;
    s->count++;
}

static Value *scope_get(Scope *s, const char *name) {
    for (Scope *cur = s; cur; cur = cur->parent) {
        for (size_t i = 0; i < cur->count; i++) {
            if (strcmp(cur->bindings[i].name, name) == 0)
                return &cur->bindings[i].value;
        }
    }
    return NULL;
}

/* Also need to update in parent scopes for assignment */
static bool scope_update(Scope *s, const char *name, Value val) {
    for (Scope *cur = s; cur; cur = cur->parent) {
        for (size_t i = 0; i < cur->count; i++) {
            if (strcmp(cur->bindings[i].name, name) == 0) {
                if (cur->bindings[i].value.type == VAL_STRING)
                    free(cur->bindings[i].value.as.str_val);
                if (cur->bindings[i].value.type == VAL_KARU)
                    karu_free(&cur->bindings[i].value.as.karu_val);
                cur->bindings[i].value = val;
                return true;
            }
        }
    }
    return false;
}

/* ── Value helpers ──────────────────────────────────── */

static Value val_int(int v)         { return (Value){VAL_INT,    {.int_val = v}}; }
static Value val_float(double v)    { return (Value){VAL_FLOAT,  {.float_val = v}}; }
static Value val_bool(bool v)       { return (Value){VAL_BOOL,   {.bool_val = v}}; }
static Value val_void(void)         { return (Value){VAL_VOID,   {.int_val = 0}}; }
static Value val_string(const char *s) {
    Value v = {VAL_STRING, {.str_val = strdup(s)}};
    return v;
}
static Value val_karu(KaruByte k) {
    Value v;
    v.type = VAL_KARU;
    v.as.karu_val = k;
    return v;
}

static bool val_truthy(Value v) {
    switch (v.type) {
    case VAL_INT:    return v.as.int_val != 0;
    case VAL_FLOAT:  return v.as.float_val != 0.0;
    case VAL_BOOL:   return v.as.bool_val;
    case VAL_STRING: return v.as.str_val && v.as.str_val[0];
    case VAL_KARU:   return v.as.karu_val.state == KARU_TRUE;
    case VAL_VOID:   return false;
    }
    return false;
}

/* ── Forward declaration ────────────────────────────── */

static Value eval_expr(Interpreter *interp, Scope *scope, ASTNode *node);
static void  exec_stmt(Interpreter *interp, Scope *scope, ASTNode *node);

/* ── Interpreter error ──────────────────────────────── */

static void interp_error(Interpreter *interp, int line, const char *msg) {
    if (interp->had_error) return;
    interp->had_error = true;
    snprintf(interp->error_msg, sizeof(interp->error_msg), "Runtime error (line %d): %s", line, msg);
}

/* ── Expression evaluation ──────────────────────────── */

static Value eval_expr(Interpreter *interp, Scope *scope, ASTNode *node) {
    if (!node || interp->had_error) return val_void();

    switch (node->type) {
    case NODE_INT_LIT:    return val_int(node->as.int_lit.int_val);
    case NODE_FLOAT_LIT:  return val_float(node->as.float_lit.float_val);
    case NODE_STRING_LIT: return val_string(node->as.string_lit.str_val);
    case NODE_BOOL_LIT:   return val_bool(node->as.bool_lit.bool_val);

    case NODE_IDENT: {
        Value *v = scope_get(scope, node->as.ident.ident_name);
        if (!v) {
            interp_error(interp, node->line, "Undefined variable");
            return val_void();
        }
        /* Clone strings and karus to avoid double-free */
        if (v->type == VAL_STRING) return val_string(v->as.str_val);
        if (v->type == VAL_KARU)   return val_karu(karu_clone(v->as.karu_val));
        return *v;
    }

    case NODE_BINARY: {
        Value left  = eval_expr(interp, scope, node->as.binary.left);
        Value right = eval_expr(interp, scope, node->as.binary.right);

        /* Int operations */
        if (left.type == VAL_INT && right.type == VAL_INT) {
            switch (node->as.binary.op) {
            case OP_ADD: return val_int(left.as.int_val + right.as.int_val);
            case OP_SUB: return val_int(left.as.int_val - right.as.int_val);
            case OP_MUL: return val_int(left.as.int_val * right.as.int_val);
            case OP_DIV:
                if (right.as.int_val == 0) { interp_error(interp, node->line, "Division by zero"); return val_int(0); }
                return val_int(left.as.int_val / right.as.int_val);
            case OP_EQ:  return val_bool(left.as.int_val == right.as.int_val);
            case OP_NEQ: return val_bool(left.as.int_val != right.as.int_val);
            case OP_LT:  return val_bool(left.as.int_val < right.as.int_val);
            case OP_GT:  return val_bool(left.as.int_val > right.as.int_val);
            case OP_LTE: return val_bool(left.as.int_val <= right.as.int_val);
            case OP_GTE: return val_bool(left.as.int_val >= right.as.int_val);
            }
        }

        /* Float operations */
        if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
            double l = (left.type == VAL_FLOAT) ? left.as.float_val : (double)left.as.int_val;
            double r = (right.type == VAL_FLOAT) ? right.as.float_val : (double)right.as.int_val;
            switch (node->as.binary.op) {
            case OP_ADD: return val_float(l + r);
            case OP_SUB: return val_float(l - r);
            case OP_MUL: return val_float(l * r);
            case OP_DIV: return val_float(l / r);
            case OP_EQ:  return val_bool(fabs(l - r) < 1e-9);
            case OP_NEQ: return val_bool(fabs(l - r) >= 1e-9);
            case OP_LT:  return val_bool(l < r);
            case OP_GT:  return val_bool(l > r);
            case OP_LTE: return val_bool(l <= r);
            case OP_GTE: return val_bool(l >= r);
            }
        }

        interp_error(interp, node->line, "Type mismatch in binary op");
        return val_void();
    }

    case NODE_UNARY: {
        Value operand = eval_expr(interp, scope, node->as.unary.operand);
        if (operand.type == VAL_INT)   return val_int(-operand.as.int_val);
        if (operand.type == VAL_FLOAT) return val_float(-operand.as.float_val);
        return val_void();
    }

    case NODE_SUPERPOSITION: {
        /* Create superposition in runtime */
        size_t n = node->as.superposition.super_count;
        if (n == 2) {
            /* Simple binary superposition */
            KaruByte k = quanti_superposition(interp->rt);
            return val_karu(k);
        }
        /* N-ary superposition: create as P(Discrete) with equal weights */
        double *probs = malloc(n * sizeof(double));
        for (size_t i = 0; i < n; i++) probs[i] = 1.0 / n;
        Distribution *d = dist_discrete(probs, NULL, n);
        free(probs);
        KaruByte k = quanti_prob(interp->rt, d);
        return val_karu(k);
    }

    case NODE_KARU_AND: {
        Value left  = eval_expr(interp, scope, node->as.binary.left);
        Value right = eval_expr(interp, scope, node->as.binary.right);
        KaruByte lk, rk;

        /* Coerce int literals to karu */
        if (left.type == VAL_KARU)  lk = left.as.karu_val;
        else if (left.type == VAL_INT) lk = left.as.int_val ? karu_true() : karu_false();
        else { interp_error(interp, node->line, "AND requires karu or int"); return val_void(); }

        if (right.type == VAL_KARU) rk = right.as.karu_val;
        else if (right.type == VAL_INT) rk = right.as.int_val ? karu_true() : karu_false();
        else { interp_error(interp, node->line, "AND requires karu or int"); return val_void(); }

        KaruByte result = quanti_and(interp->rt, lk, rk);
        return val_karu(result);
    }

    case NODE_KARU_OR: {
        Value left  = eval_expr(interp, scope, node->as.binary.left);
        Value right = eval_expr(interp, scope, node->as.binary.right);
        KaruByte lk, rk;

        if (left.type == VAL_KARU)  lk = left.as.karu_val;
        else if (left.type == VAL_INT) lk = left.as.int_val ? karu_true() : karu_false();
        else { interp_error(interp, node->line, "OR requires karu or int"); return val_void(); }

        if (right.type == VAL_KARU) rk = right.as.karu_val;
        else if (right.type == VAL_INT) rk = right.as.int_val ? karu_true() : karu_false();
        else { interp_error(interp, node->line, "OR requires karu or int"); return val_void(); }

        KaruByte result = quanti_or(interp->rt, lk, rk);
        return val_karu(result);
    }

    case NODE_KARU_NOT: {
        Value operand = eval_expr(interp, scope, node->as.unary.operand);
        KaruByte k;
        if (operand.type == VAL_KARU)  k = operand.as.karu_val;
        else if (operand.type == VAL_INT) k = operand.as.int_val ? karu_true() : karu_false();
        else { interp_error(interp, node->line, "NOT requires karu or int"); return val_void(); }

        KaruByte result = quanti_not(interp->rt, k);
        return val_karu(result);
    }

    case NODE_MEASURE: {
        Value expr = eval_expr(interp, scope, node->as.measure.measure_expr);
        if (expr.type != VAL_KARU) {
            interp_error(interp, node->line, "measure requires karu value");
            return val_void();
        }

        CollapseMode mode;
        switch (node->as.measure.measure_mode) {
        case MEASURE_MAP:    mode = COLLAPSE_MAP;    break;
        case MEASURE_SAMPLE: mode = COLLAPSE_SAMPLE; break;
        case MEASURE_FIRST:  mode = COLLAPSE_FIRST;  break;
        default:             mode = COLLAPSE_MAP;    break;
        }

        /* MAP sobre discreta con etiquetas → devolver la etiqueta */
        if (mode == COLLAPSE_MAP && expr.as.karu_val.state == KARU_PROB && expr.as.karu_val.dist) {
            const char *label = dist_map_label(expr.as.karu_val.dist);
            if (label) {
                Value result = val_string(label);  /* strdup ANTES de karu_free */
                karu_free(&expr.as.karu_val);
                return result;
            }
        }

        KaruByte collapsed = quanti_measure(interp->rt, expr.as.karu_val, mode);
        /* Return as int: TRUE=1, FALSE=0 */
        int result = (collapsed.state == KARU_TRUE) ? 1 : 0;
        karu_free(&collapsed);
        return val_int(result);
    }

    case NODE_P_DIST: {
        Distribution *d = NULL;

        if (node->as.p_dist.dist_type == DIST_AST_NORMAL && node->as.p_dist.dist_arg_count == 2) {
            Value mean   = eval_expr(interp, scope, node->as.p_dist.dist_args[0]);
            Value stddev = eval_expr(interp, scope, node->as.p_dist.dist_args[1]);
            double m = (mean.type == VAL_FLOAT) ? mean.as.float_val : (double)mean.as.int_val;
            double s = (stddev.type == VAL_FLOAT) ? stddev.as.float_val : (double)stddev.as.int_val;
            d = dist_normal(m, s);
        }
        else if (node->as.p_dist.dist_type == DIST_AST_UNIFORM && node->as.p_dist.dist_arg_count == 2) {
            Value min_v = eval_expr(interp, scope, node->as.p_dist.dist_args[0]);
            Value max_v = eval_expr(interp, scope, node->as.p_dist.dist_args[1]);
            double mn = (min_v.type == VAL_FLOAT) ? min_v.as.float_val : (double)min_v.as.int_val;
            double mx = (max_v.type == VAL_FLOAT) ? max_v.as.float_val : (double)max_v.as.int_val;
            d = dist_uniform(mn, mx);
        }
        else if (node->as.p_dist.dist_type == DIST_AST_DISCRETE && node->as.p_dist.dist_arg_count >= 1) {
            /* First arg should be array of probabilities */
            ASTNode *probs_node = node->as.p_dist.dist_args[0];
            if (probs_node->type == NODE_ARRAY_LIT) {
                size_t n = probs_node->as.array_lit.array_count;
                double *probs = malloc(n * sizeof(double));
                for (size_t i = 0; i < n; i++) {
                    Value pv = eval_expr(interp, scope, probs_node->as.array_lit.array_elems[i]);
                    probs[i] = (pv.type == VAL_FLOAT) ? pv.as.float_val : (double)pv.as.int_val;
                }

                /* Labels (optional second arg) */
                char **labels = NULL;
                if (node->as.p_dist.dist_arg_count >= 2) {
                    ASTNode *labels_node = node->as.p_dist.dist_args[1];
                    if (labels_node->type == NODE_ARRAY_LIT) {
                        labels = malloc(n * sizeof(char *));
                        for (size_t i = 0; i < n && i < labels_node->as.array_lit.array_count; i++) {
                            Value lv = eval_expr(interp, scope, labels_node->as.array_lit.array_elems[i]);
                            labels[i] = (lv.type == VAL_STRING) ? lv.as.str_val : strdup("?");
                        }
                    }
                }

                d = dist_discrete(probs, (const char **)labels, n);
                free(probs);
                if (labels) {
                    for (size_t i = 0; i < n; i++) free(labels[i]);
                    free(labels);
                }
            }
        }

        if (!d) {
            interp_error(interp, node->line, "Invalid distribution");
            return val_void();
        }

        KaruByte k = quanti_prob(interp->rt, d);
        return val_karu(k);
    }

    case NODE_CALL: {
        /* Find function */
        FnEntry *fn = NULL;
        for (size_t i = 0; i < interp->fn_count; i++) {
            if (strcmp(interp->functions[i].name, node->as.call.call_name) == 0) {
                fn = &interp->functions[i];
                break;
            }
        }
        if (!fn) {
            interp_error(interp, node->line, "Undefined function");
            return val_void();
        }

        ASTNode *decl = fn->decl;
        if (node->as.call.call_arg_count != decl->as.fn_decl.fn_param_count) {
            interp_error(interp, node->line, "Wrong number of arguments");
            return val_void();
        }

        /* Create new scope for function */
        Scope *fn_scope = scope_create(interp->global_scope);
        for (size_t i = 0; i < node->as.call.call_arg_count; i++) {
            Value arg = eval_expr(interp, scope, node->as.call.call_args[i]);
            scope_set(fn_scope, decl->as.fn_decl.fn_params[i].name, arg);
        }

        /* Execute body */
        interp->returning = false;
        ASTNode *body = decl->as.fn_decl.fn_body;
        for (size_t i = 0; i < body->as.block.block_count; i++) {
            exec_stmt(interp, fn_scope, body->as.block.block_stmts[i]);
            if (interp->returning || interp->had_error) break;
        }

        scope_free(fn_scope);

        if (interp->returning) {
            interp->returning = false;
            return interp->return_value;
        }
        return val_void();
    }

    default:
        interp_error(interp, node->line, "Unsupported expression type");
        return val_void();
    }
}

/* ── Print value ────────────────────────────────────── */

static void print_value(Value v) {
    switch (v.type) {
    case VAL_INT:    printf("%d\n", v.as.int_val); break;
    case VAL_FLOAT:  printf("%g\n", v.as.float_val); break;
    case VAL_STRING: printf("%s\n", v.as.str_val); break;
    case VAL_BOOL:   printf("%s\n", v.as.bool_val ? "true" : "false"); break;
    case VAL_KARU:   printf("%s\n", karu_state_name(v.as.karu_val.state)); break;
    case VAL_VOID:   printf("void\n"); break;
    }
}

static bool karu_is_multistate(KaruByte k) {
    return k.state == KARU_SUPER || k.state == KARU_PROB || k.state == KARU_UNDEF;
}

/* Force collapse for I/O (measure:map). Labels preferred when present. */
static Value collapse_for_io(Interpreter *interp, Value v) {
    if (v.type != VAL_KARU) return v;
    if (!karu_is_multistate(v.as.karu_val) && v.as.karu_val.state != KARU_TRUE
        && v.as.karu_val.state != KARU_FALSE) {
        return v;
    }

    if (v.as.karu_val.state == KARU_TRUE) {
        karu_free(&v.as.karu_val);
        return val_int(1);
    }
    if (v.as.karu_val.state == KARU_FALSE) {
        karu_free(&v.as.karu_val);
        return val_int(0);
    }

    if (v.as.karu_val.state == KARU_PROB && v.as.karu_val.dist) {
        const char *label = dist_map_label(v.as.karu_val.dist);
        if (label) {
            Value result = val_string(label);
            karu_free(&v.as.karu_val);
            return result;
        }
    }

    KaruByte collapsed = quanti_measure(interp->rt, v.as.karu_val, COLLAPSE_MAP);
    int result = (collapsed.state == KARU_TRUE) ? 1 : 0;
    karu_free(&collapsed);
    return val_int(result);
}

/* Deep-clone a scope's bindings (shallow parent link to same parent). */
static Scope *scope_clone_shallow(Scope *src) {
    Scope *s = scope_create(src->parent);
    for (size_t i = 0; i < src->count; i++) {
        Value v = src->bindings[i].value;
        if (v.type == VAL_STRING) v = val_string(v.as.str_val);
        else if (v.type == VAL_KARU) v = val_karu(karu_clone(v.as.karu_val));
        scope_set(s, src->bindings[i].name, v);
    }
    return s;
}

/* Merge integer/bool assignments from forked scopes into parent.
 * Divergent ints → MAP (highest-weight world wins) for classical slots.
 * Divergent karus → superposition. */
static void merge_forked_scopes(Interpreter *interp, Scope *parent,
                                Scope **worlds, double *weights, size_t n_worlds,
                                const char **tracked, size_t n_tracked) {
    (void)interp;
    for (size_t t = 0; t < n_tracked; t++) {
        const char *name = tracked[t];
        double best_w = -1.0;
        Value best = {VAL_VOID, {.int_val = 0}};
        bool have = false;
        bool all_same = true;
        Value first = {VAL_VOID, {.int_val = 0}};

        for (size_t w = 0; w < n_worlds; w++) {
            Value *v = scope_get(worlds[w], name);
            if (!v) continue;

            if (!have) {
                first = *v;
                if (first.type == VAL_STRING)
                    first = val_string(v->as.str_val);
                else if (first.type == VAL_KARU)
                    first = val_karu(karu_clone(v->as.karu_val));
                have = true;
                best = first;
                best_w = weights[w];
            } else {
                bool same = false;
                if (first.type == VAL_INT && v->type == VAL_INT)
                    same = first.as.int_val == v->as.int_val;
                else if (first.type == VAL_BOOL && v->type == VAL_BOOL)
                    same = first.as.bool_val == v->as.bool_val;
                if (!same) all_same = false;

                if (weights[w] >= best_w) {
                    if (best.type == VAL_STRING) free(best.as.str_val);
                    else if (best.type == VAL_KARU) karu_free(&best.as.karu_val);
                    best_w = weights[w];
                    if (v->type == VAL_STRING) best = val_string(v->as.str_val);
                    else if (v->type == VAL_KARU) best = val_karu(karu_clone(v->as.karu_val));
                    else best = *v;
                }
            }
        }

        if (!have) continue;

        if (all_same) {
            if (first.type == VAL_STRING)
                scope_update(parent, name, val_string(first.as.str_val));
            else if (first.type == VAL_KARU)
                scope_update(parent, name, val_karu(karu_clone(first.as.karu_val)));
            else
                scope_update(parent, name, first);
            if (first.type == VAL_STRING) free(first.as.str_val);
            else if (first.type == VAL_KARU) karu_free(&first.as.karu_val);
            if (best.type == VAL_STRING && best.as.str_val != first.as.str_val)
                free(best.as.str_val);
            else if (best.type == VAL_KARU)
                karu_free(&best.as.karu_val);
        } else {
            /* MAP merge: keep highest-weight world's value */
            if (best.type == VAL_STRING)
                scope_update(parent, name, best);
            else if (best.type == VAL_KARU)
                scope_update(parent, name, best);
            else
                scope_update(parent, name, best);
            if (first.type == VAL_STRING) free(first.as.str_val);
            else if (first.type == VAL_KARU) karu_free(&first.as.karu_val);
        }
    }
}

static void exec_block_stmts(Interpreter *interp, Scope *scope, ASTNode *block) {
    if (!block || block->type != NODE_BLOCK) return;
    for (size_t i = 0; i < block->as.block.block_count; i++) {
        exec_stmt(interp, scope, block->as.block.block_stmts[i]);
        if (interp->returning || interp->had_error) break;
    }
}

static void exec_multistate_if(Interpreter *interp, Scope *scope, ASTNode *node, Value cond) {
    branch_clear(interp->rt->branches);
    size_t created = quanti_fork(interp->rt, cond.as.karu_val);
    if (created == 0) {
        /* Fallback: collapse then classical if */
        Value collapsed = collapse_for_io(interp, cond);
        bool truthy = val_truthy(collapsed);
        if (collapsed.type == VAL_STRING) free(collapsed.as.str_val);
        ASTNode *block = truthy ? node->as.if_stmt.if_then : node->as.if_stmt.if_else;
        if (block) {
            Scope *inner = scope_create(scope);
            exec_block_stmts(interp, inner, block);
            scope_free(inner);
        }
        return;
    }

    quanti_prune(interp->rt);

    /* Collect active branches */
    size_t n = 0;
    for (size_t i = 0; i < interp->rt->branches->count; i++)
        if (interp->rt->branches->branches[i].active) n++;

    Scope **worlds = calloc(n, sizeof(Scope *));
    double *weights = calloc(n, sizeof(double));
    size_t wi = 0;

    const char *tracked_buf[64];
    size_t n_tracked = 0;

    for (size_t i = 0; i < interp->rt->branches->count; i++) {
        Branch *b = &interp->rt->branches->branches[i];
        if (!b->active) continue;

        KaruByte *local = branch_get_local(interp->rt->branches, b->id, cond.as.karu_val.id);
        bool take_then = local && local->state == KARU_TRUE;
        ASTNode *block = take_then ? node->as.if_stmt.if_then : node->as.if_stmt.if_else;
        if (!block) {
            worlds[wi] = scope_clone_shallow(scope);
            weights[wi] = b->weight;
            wi++;
            continue;
        }

        Scope *world = scope_clone_shallow(scope);
        /* Bind condition name if it was an ident — skip; use local override via karu in scope if present */
        exec_block_stmts(interp, world, block);

        /* Track names assigned in this world that exist in parent */
        for (size_t j = 0; j < world->count; j++) {
            const char *nm = world->bindings[j].name;
            if (scope_get(scope, nm)) {
                bool already = false;
                for (size_t k = 0; k < n_tracked; k++)
                    if (strcmp(tracked_buf[k], nm) == 0) { already = true; break; }
                if (!already && n_tracked < 64)
                    tracked_buf[n_tracked++] = nm;
            }
        }

        worlds[wi] = world;
        weights[wi] = b->weight;
        wi++;
    }

    merge_forked_scopes(interp, scope, worlds, weights, wi, tracked_buf, n_tracked);

    for (size_t i = 0; i < wi; i++) scope_free(worlds[i]);
    free(worlds);
    free(weights);
    karu_free(&cond.as.karu_val);
}

static void exec_multistate_when(Interpreter *interp, Scope *scope, ASTNode *node, Value cond) {
    branch_clear(interp->rt->branches);
    size_t created = quanti_fork(interp->rt, cond.as.karu_val);
    if (created == 0) {
        Value collapsed = collapse_for_io(interp, cond);
        if (val_truthy(collapsed)) {
            Scope *inner = scope_create(scope);
            exec_block_stmts(interp, inner, node->as.when_stmt.when_body);
            scope_free(inner);
        }
        if (collapsed.type == VAL_STRING) free(collapsed.as.str_val);
        return;
    }

    quanti_prune(interp->rt);

    for (size_t i = 0; i < interp->rt->branches->count; i++) {
        Branch *b = &interp->rt->branches->branches[i];
        if (!b->active) continue;
        KaruByte *local = branch_get_local(interp->rt->branches, b->id, cond.as.karu_val.id);
        if (local && local->state == KARU_TRUE) {
            Scope *inner = scope_create(scope);
            exec_block_stmts(interp, inner, node->as.when_stmt.when_body);
            scope_free(inner);
        }
    }
    karu_free(&cond.as.karu_val);
}

/* ── Statement execution ────────────────────────────── */

static void exec_stmt(Interpreter *interp, Scope *scope, ASTNode *node) {
    if (!node || interp->had_error || interp->returning) return;

    switch (node->type) {
    case NODE_VAR_DECL: {
        Value init = val_void();
        if (node->as.var_decl.var_init)
            init = eval_expr(interp, scope, node->as.var_decl.var_init);

        /* If karu and persistent, mark it */
        if (init.type == VAL_KARU && node->as.var_decl.var_persistent)
            init.as.karu_val.persistent = true;

        scope_set(scope, node->as.var_decl.var_name, init);
        break;
    }

    case NODE_ASSIGN: {
        Value val = eval_expr(interp, scope, node->as.assign.assign_value);
        if (!scope_update(scope, node->as.assign.assign_name, val))
            interp_error(interp, node->line, "Undefined variable in assignment");
        break;
    }

    case NODE_PRINT: {
        Value v = eval_expr(interp, scope, node->as.print.print_expr);
        /* I/O forces collapse (measure:map) for multi-state karu */
        if (v.type == VAL_KARU && karu_is_multistate(v.as.karu_val)) {
            Value collapsed = collapse_for_io(interp, v);
            print_value(collapsed);
            if (collapsed.type == VAL_STRING) free(collapsed.as.str_val);
        } else {
            print_value(v);
            if (v.type == VAL_STRING) free(v.as.str_val);
            else if (v.type == VAL_KARU) karu_free(&v.as.karu_val);
        }
        break;
    }

    case NODE_IF: {
        Value cond = eval_expr(interp, scope, node->as.if_stmt.if_cond);
        if (cond.type == VAL_KARU && karu_is_multistate(cond.as.karu_val)) {
            exec_multistate_if(interp, scope, node, cond);
        } else {
            if (val_truthy(cond)) {
                Scope *inner = scope_create(scope);
                exec_block_stmts(interp, inner, node->as.if_stmt.if_then);
                scope_free(inner);
            } else if (node->as.if_stmt.if_else) {
                Scope *inner = scope_create(scope);
                exec_block_stmts(interp, inner, node->as.if_stmt.if_else);
                scope_free(inner);
            }
            if (cond.type == VAL_STRING) free(cond.as.str_val);
            else if (cond.type == VAL_KARU) karu_free(&cond.as.karu_val);
        }
        break;
    }

    case NODE_WHEN: {
        Value cond = eval_expr(interp, scope, node->as.when_stmt.when_cond);
        if (cond.type == VAL_KARU && karu_is_multistate(cond.as.karu_val)) {
            exec_multistate_when(interp, scope, node, cond);
        } else {
            if (val_truthy(cond)) {
                Scope *inner = scope_create(scope);
                exec_block_stmts(interp, inner, node->as.when_stmt.when_body);
                scope_free(inner);
            }
            if (cond.type == VAL_STRING) free(cond.as.str_val);
            else if (cond.type == VAL_KARU) karu_free(&cond.as.karu_val);
        }
        break;
    }

    case NODE_RUNTIME_CFG: {
        if (node->as.runtime_cfg.has_max_branches) {
            interp->rt->branches->max_branches = node->as.runtime_cfg.max_branches;
            interp->rt->pruner.max_active = node->as.runtime_cfg.max_branches;
        }
        if (node->as.runtime_cfg.has_prune_threshold) {
            interp->rt->branches->prune_threshold = node->as.runtime_cfg.prune_threshold;
            interp->rt->pruner.weight_threshold = node->as.runtime_cfg.prune_threshold;
        }
        break;
    }

    case NODE_WHILE: {
        Scope *inner = scope_create(scope);
        int limit = 10000; /* safety */
        while (!interp->had_error && !interp->returning && limit-- > 0) {
            Value cond = eval_expr(interp, inner, node->as.while_stmt.while_cond);
            bool ok = val_truthy(cond);
            if (cond.type == VAL_STRING) free(cond.as.str_val);
            else if (cond.type == VAL_KARU) {
                if (karu_is_multistate(cond.as.karu_val)) {
                    Value c = collapse_for_io(interp, cond);
                    ok = val_truthy(c);
                    if (c.type == VAL_STRING) free(c.as.str_val);
                } else {
                    karu_free(&cond.as.karu_val);
                }
            }
            if (!ok) break;
            exec_block_stmts(interp, inner, node->as.while_stmt.while_body);
        }
        scope_free(inner);
        break;
    }

    case NODE_FN_DECL: {
        /* Register function */
        if (interp->fn_count >= interp->fn_cap) {
            interp->fn_cap *= 2;
            interp->functions = realloc(interp->functions, interp->fn_cap * sizeof(FnEntry));
        }
        interp->functions[interp->fn_count].name = strdup(node->as.fn_decl.fn_name);
        interp->functions[interp->fn_count].decl = node;
        interp->fn_count++;
        break;
    }

    case NODE_RETURN: {
        interp->return_value = node->as.ret.ret_expr
            ? eval_expr(interp, scope, node->as.ret.ret_expr)
            : val_void();
        interp->returning = true;
        break;
    }

    case NODE_BLOCK: {
        Scope *inner = scope_create(scope);
        exec_block_stmts(interp, inner, node);
        scope_free(inner);
        break;
    }

    case NODE_EXPR_STMT:
        eval_expr(interp, scope, node->as.expr_stmt.expr);
        break;

    default:
        interp_error(interp, node->line, "Unsupported statement type");
        break;
    }
}

/* ── API ────────────────────────────────────────────── */

Interpreter *interp_create(QuantiConfig config) {
    Interpreter *interp = calloc(1, sizeof(Interpreter));
    if (!interp) return NULL;

    interp->rt = quanti_init(config);
    interp->global_scope = scope_create(NULL);
    interp->fn_cap = 16;
    interp->functions = calloc(interp->fn_cap, sizeof(FnEntry));
    interp->returning = false;

    return interp;
}

void interp_destroy(Interpreter *interp) {
    if (!interp) return;
    quanti_destroy(interp->rt);
    scope_free(interp->global_scope);
    for (size_t i = 0; i < interp->fn_count; i++)
        free(interp->functions[i].name);
    free(interp->functions);
    free(interp);
}

bool interp_run(Interpreter *interp, Program *prog) {
    for (size_t i = 0; i < prog->count; i++) {
        exec_stmt(interp, interp->global_scope, prog->stmts[i]);
        if (interp->had_error) {
            fprintf(stderr, "%s\n", interp->error_msg);
            return false;
        }
    }
    return true;
}

bool interp_exec(Interpreter *interp, const char *source) {
    Parser p;
    parser_init(&p, source);
    Program prog = parser_parse(&p);

    if (p.had_error) {
        fprintf(stderr, "Parse error: %s\n", p.error_msg);
        program_free(&prog);
        return false;
    }

    bool ok = interp_run(interp, &prog);
    program_free(&prog);
    return ok;
}
