#include "vm.h"
#include "distribution.h"
#include "collapse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef enum {
    V_INT,
    V_FLOAT,
    V_STRING,
    V_BOOL,
    V_KARU,
    V_VOID,
} VTag;

typedef struct {
    VTag tag;
    union {
        int       i;
        double    f;
        char     *s;
        bool      b;
        KaruByte  k;
    } as;
} V;

#define STACK_MAX 4096
#define CALL_MAX  256
#define LOCAL_MAX 512

static V v_int(int x) { return (V){V_INT, {.i = x}}; }
static V v_float(double x) { return (V){V_FLOAT, {.f = x}}; }
static V v_bool(bool x) { return (V){V_BOOL, {.b = x}}; }
static V v_void(void) { return (V){V_VOID, {.i = 0}}; }
static V v_string_dup(const char *s) { return (V){V_STRING, {.s = strdup(s ? s : "")}}; }
static V v_karu(KaruByte k) { return (V){V_KARU, {.k = k}}; }

static void v_free(V *v) {
    if (!v) return;
    if (v->tag == V_STRING) free(v->as.s);
    else if (v->tag == V_KARU) karu_free(&v->as.k);
    v->tag = V_VOID;
}

static bool karu_multistate(KaruByte k) {
    return k.state == KARU_SUPER || k.state == KARU_PROB || k.state == KARU_UNDEF;
}

static KaruByte v_to_karu(V v) {
    if (v.tag == V_KARU) return v.as.k;
    if (v.tag == V_INT) return v.as.i ? karu_true() : karu_false();
    if (v.tag == V_BOOL) return v.as.b ? karu_true() : karu_false();
    return karu_false();
}

static bool v_truthy(QuantiRuntime *rt, V v) {
    (void)rt;
    switch (v.tag) {
    case V_INT:    return v.as.i != 0;
    case V_FLOAT:  return v.as.f != 0.0;
    case V_BOOL:   return v.as.b;
    case V_STRING: return v.as.s && v.as.s[0];
    case V_KARU:
        if (karu_multistate(v.as.k)) {
            KaruByte c = collapse_single(v.as.k, COLLAPSE_MAP);
            bool t = c.state == KARU_TRUE;
            karu_free(&c);
            return t;
        }
        return v.as.k.state == KARU_TRUE;
    default:       return false;
    }
}

static V collapse_for_io(QuantiRuntime *rt, V v) {
    if (v.tag != V_KARU) return v;

    if (v.as.k.state == KARU_TRUE) {
        karu_free(&v.as.k);
        return v_int(1);
    }
    if (v.as.k.state == KARU_FALSE) {
        karu_free(&v.as.k);
        return v_int(0);
    }
    if (!karu_multistate(v.as.k)) return v;

    if (v.as.k.state == KARU_PROB && v.as.k.dist) {
        const char *label = dist_map_label(v.as.k.dist);
        if (label) {
            V result = v_string_dup(label);
            karu_free(&v.as.k);
            return result;
        }
    }

    KaruByte collapsed = quanti_measure(rt, v.as.k, COLLAPSE_MAP);
    int result = (collapsed.state == KARU_TRUE) ? 1 : 0;
    karu_free(&collapsed);
    return v_int(result);
}

static void print_v(V v) {
    switch (v.tag) {
    case V_INT:    printf("%d\n", v.as.i); break;
    case V_FLOAT:  printf("%g\n", v.as.f); break;
    case V_STRING: printf("%s\n", v.as.s); break;
    case V_BOOL:   printf("%s\n", v.as.b ? "true" : "false"); break;
    case V_KARU:   printf("%s\n", karu_state_name(v.as.k.state)); break;
    default:       printf("void\n"); break;
    }
}

static V pop(V *stack, int *sp) {
    if (*sp <= 0) return v_void();
    return stack[--(*sp)];
}

static void push(V *stack, int *sp, V v) {
    if (*sp >= STACK_MAX) return;
    stack[(*sp)++] = v;
}

static V binop(IrOpcode op, V a, V b, int line, bool *err) {
    if (a.tag == V_INT && b.tag == V_INT) {
        switch (op) {
        case IR_ADD: return v_int(a.as.i + b.as.i);
        case IR_SUB: return v_int(a.as.i - b.as.i);
        case IR_MUL: return v_int(a.as.i * b.as.i);
        case IR_DIV:
            if (b.as.i == 0) { *err = true; (void)line; return v_int(0); }
            return v_int(a.as.i / b.as.i);
        case IR_EQ:  return v_bool(a.as.i == b.as.i);
        case IR_NEQ: return v_bool(a.as.i != b.as.i);
        case IR_LT:  return v_bool(a.as.i < b.as.i);
        case IR_GT:  return v_bool(a.as.i > b.as.i);
        case IR_LTE: return v_bool(a.as.i <= b.as.i);
        case IR_GTE: return v_bool(a.as.i >= b.as.i);
        default: break;
        }
    }
    double l = (a.tag == V_FLOAT) ? a.as.f : (a.tag == V_INT) ? (double)a.as.i : 0.0;
    double r = (b.tag == V_FLOAT) ? b.as.f : (b.tag == V_INT) ? (double)b.as.i : 0.0;
    if (a.tag == V_INT || a.tag == V_FLOAT || b.tag == V_INT || b.tag == V_FLOAT) {
        switch (op) {
        case IR_ADD: return v_float(l + r);
        case IR_SUB: return v_float(l - r);
        case IR_MUL: return v_float(l * r);
        case IR_DIV: return v_float(l / r);
        case IR_EQ:  return v_bool(fabs(l - r) < 1e-9);
        case IR_NEQ: return v_bool(fabs(l - r) >= 1e-9);
        case IR_LT:  return v_bool(l < r);
        case IR_GT:  return v_bool(l > r);
        case IR_LTE: return v_bool(l <= r);
        case IR_GTE: return v_bool(l >= r);
        default: break;
        }
    }
    (void)line;
    *err = true;
    return v_void();
}

bool vm_execute(IrModule *m, QuantiConfig config) {
    if (!m || m->had_error) return false;

    QuantiRuntime *rt = quanti_init(config);
    if (!rt) return false;

    V stack[STACK_MAX];
    V locals[LOCAL_MAX];
    int sp = 0;
    size_t ip = 0;
    size_t ret_stack[CALL_MAX];
    int ret_sp = 0;
    bool failed = false;

    while (ip < m->count && !failed) {
        const IrInst *in = &m->code[ip++];

        switch (in->op) {
        case IR_NOP:
            break;

        case IR_CONST_I:
            push(stack, &sp, v_int(in->operand));
            break;

        case IR_CONST_F:
            push(stack, &sp, v_float(in->fimm));
            break;

        case IR_CONST_S:
            if (in->operand >= 0 && (size_t)in->operand < m->string_count)
                push(stack, &sp, v_string_dup(m->strings[in->operand]));
            else
                push(stack, &sp, v_string_dup(""));
            break;

        case IR_CONST_B:
            push(stack, &sp, v_bool(in->operand != 0));
            break;

        case IR_LOAD:
            if (in->operand >= 0 && in->operand < LOCAL_MAX) {
                V lv = locals[in->operand];
                if (lv.tag == V_STRING) push(stack, &sp, v_string_dup(lv.as.s));
                else if (lv.tag == V_KARU) push(stack, &sp, v_karu(karu_clone(lv.as.k)));
                else push(stack, &sp, lv);
            }
            break;

        case IR_STORE: {
            V val = pop(stack, &sp);
            if (in->operand >= 0 && in->operand < LOCAL_MAX) {
                v_free(&locals[in->operand]);
                locals[in->operand] = val;
            } else v_free(&val);
            break;
        }

        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV:
        case IR_EQ: case IR_NEQ: case IR_LT: case IR_GT: case IR_LTE: case IR_GTE: {
            V b = pop(stack, &sp);
            V a = pop(stack, &sp);
            V r = binop(in->op, a, b, in->line, &failed);
            v_free(&a);
            v_free(&b);
            push(stack, &sp, r);
            break;
        }

        case IR_NEG: {
            V o = pop(stack, &sp);
            if (o.tag == V_INT) push(stack, &sp, v_int(-o.as.i));
            else if (o.tag == V_FLOAT) push(stack, &sp, v_float(-o.as.f));
            else { v_free(&o); failed = true; }
            break;
        }

        case IR_JMP:
            ip = (size_t)in->operand;
            break;

        case IR_JMP_IF: {
            V c = pop(stack, &sp);
            if (v_truthy(rt, c)) ip = (size_t)in->operand;
            v_free(&c);
            break;
        }

        case IR_JMP_IF_NOT: {
            V c = pop(stack, &sp);
            if (!v_truthy(rt, c)) ip = (size_t)in->operand;
            v_free(&c);
            break;
        }

        case IR_PRINT: {
            V v = pop(stack, &sp);
            if (v.tag == V_KARU && karu_multistate(v.as.k)) {
                V c = collapse_for_io(rt, v);
                print_v(c);
                v_free(&c);
            } else {
                print_v(v);
                v_free(&v);
            }
            break;
        }

        case IR_POP: {
            V v = pop(stack, &sp);
            v_free(&v);
            break;
        }

        case IR_CALL: {
            if (in->operand < 0 || (size_t)in->operand >= m->func_count) {
                failed = true;
                break;
            }
            IrFunc *fn = &m->funcs[in->operand];
            for (int a = in->aux - 1; a >= 0; a--) {
                V arg = pop(stack, &sp);
                if (a >= 0 && fn->param_slots[a] >= 0 && fn->param_slots[a] < LOCAL_MAX) {
                    v_free(&locals[fn->param_slots[a]]);
                    locals[fn->param_slots[a]] = arg;
                } else v_free(&arg);
            }
            if (ret_sp < CALL_MAX)
                ret_stack[ret_sp++] = ip;
            ip = (size_t)fn->entry_ip;
            break;
        }

        case IR_RET:
            if (ret_sp > 0)
                ip = ret_stack[--ret_sp];
            else
                ip = m->count;
            break;

        case IR_KARU_SUPER: {
            KaruByte k;
            if (in->aux == 2)
                k = quanti_superposition(rt);
            else {
                size_t n = (size_t)in->aux;
                if (n < 2) n = 2;
                double *probs = malloc(n * sizeof(double));
                for (size_t i = 0; i < n; i++) probs[i] = 1.0 / (double)n;
                Distribution *d = dist_discrete(probs, NULL, n);
                free(probs);
                k = quanti_prob(rt, d);
            }
            push(stack, &sp, v_karu(k));
            break;
        }

        case IR_KARU_AND: {
            V b = pop(stack, &sp);
            V a = pop(stack, &sp);
            KaruByte kb = quanti_and(rt, v_to_karu(a), v_to_karu(b));
            v_free(&a);
            v_free(&b);
            push(stack, &sp, v_karu(kb));
            break;
        }

        case IR_KARU_OR: {
            V b = pop(stack, &sp);
            V a = pop(stack, &sp);
            KaruByte kb = quanti_or(rt, v_to_karu(a), v_to_karu(b));
            v_free(&a);
            v_free(&b);
            push(stack, &sp, v_karu(kb));
            break;
        }

        case IR_KARU_NOT: {
            V a = pop(stack, &sp);
            KaruByte kb = quanti_not(rt, v_to_karu(a));
            v_free(&a);
            push(stack, &sp, v_karu(kb));
            break;
        }

        case IR_MEASURE: {
            V e = pop(stack, &sp);
            CollapseMode mode = COLLAPSE_MAP;
            switch ((MeasureMode)in->operand) {
            case MEASURE_MAP:    mode = COLLAPSE_MAP; break;
            case MEASURE_SAMPLE: mode = COLLAPSE_SAMPLE; break;
            case MEASURE_FIRST:  mode = COLLAPSE_FIRST; break;
            default: break;
            }
            if (e.tag != V_KARU) {
                v_free(&e);
                failed = true;
                break;
            }
            if (mode == COLLAPSE_MAP && e.as.k.state == KARU_PROB && e.as.k.dist) {
                const char *label = dist_map_label(e.as.k.dist);
                if (label) {
                    push(stack, &sp, v_string_dup(label));
                    karu_free(&e.as.k);
                    break;
                }
            }
            KaruByte c = quanti_measure(rt, e.as.k, mode);
            int r = (c.state == KARU_TRUE) ? 1 : 0;
            karu_free(&c);
            push(stack, &sp, v_int(r));
            break;
        }

        case IR_P_NORMAL: {
            V sd = pop(stack, &sp);
            V mn = pop(stack, &sp);
            double mean = (mn.tag == V_FLOAT) ? mn.as.f : (double)mn.as.i;
            double stddev = (sd.tag == V_FLOAT) ? sd.as.f : (double)sd.as.i;
            v_free(&mn);
            v_free(&sd);
            Distribution *d = dist_normal(mean, stddev);
            push(stack, &sp, v_karu(quanti_prob(rt, d)));
            break;
        }

        case IR_P_UNIFORM: {
            V mx = pop(stack, &sp);
            V mn = pop(stack, &sp);
            double min_v = (mn.tag == V_FLOAT) ? mn.as.f : (double)mn.as.i;
            double max_v = (mx.tag == V_FLOAT) ? mx.as.f : (double)mx.as.i;
            v_free(&mn);
            v_free(&mx);
            Distribution *d = dist_uniform(min_v, max_v);
            push(stack, &sp, v_karu(quanti_prob(rt, d)));
            break;
        }

        case IR_P_DISCRETE: {
            size_t n = (size_t)in->operand;
            double *probs = malloc(n * sizeof(double));
            for (size_t i = n; i > 0; i--) {
                V pv = pop(stack, &sp);
                probs[i - 1] = (pv.tag == V_FLOAT) ? pv.as.f : (double)pv.as.i;
            }
            char **labels = NULL;
            if (in->aux >= 0 && (size_t)in->aux < m->string_count) {
                labels = calloc(n, sizeof(char *));
                for (size_t i = 0; i < n; i++) {
                    size_t si = (size_t)in->aux + i;
                    if (si < m->string_count)
                        labels[i] = strdup(m->strings[si]);
                }
            }
            Distribution *d = dist_discrete(probs, (const char **)labels, n);
            free(probs);
            if (labels) {
                for (size_t i = 0; i < n; i++) free(labels[i]);
                free(labels);
            }
            push(stack, &sp, v_karu(quanti_prob(rt, d)));
            break;
        }

        case IR_FORK_IF: {
            V c = pop(stack, &sp);
            if (c.tag == V_KARU && karu_multistate(c.as.k))
                c = collapse_for_io(rt, c);
            if (!v_truthy(rt, c)) ip = (size_t)in->operand;
            v_free(&c);
            break;
        }

        case IR_RUNTIME_CFG:
            if (in->aux & 1) {
                rt->branches->max_branches = (size_t)in->operand;
                rt->pruner.max_active = (size_t)in->operand;
            }
            if (in->aux & 2) {
                rt->branches->prune_threshold = in->fimm;
                rt->pruner.weight_threshold = in->fimm;
            }
            break;

        case IR_HALT:
            ip = m->count;
            break;

        default:
            failed = true;
            break;
        }
    }

    for (int i = 0; i < sp; i++) v_free(&stack[i]);
    for (int i = 0; i < LOCAL_MAX; i++) v_free(&locals[i]);

    quanti_destroy(rt);
    return !failed;
}
