#include "specialize.h"

static bool is_karu_op(IrOpcode op) {
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
        return true;
    default:
        return false;
    }
}

void specialize_module(IrModule *m) {
    if (!m) return;

    m->uses_karu = false;
    for (size_t i = 0; i < m->count; i++) {
        bool karu = is_karu_op(m->code[i].op);
        m->code[i].classical = !karu;
        if (karu) m->uses_karu = true;
    }
    m->all_classical = !m->uses_karu;
}
