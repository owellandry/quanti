#include "runtime.h"
#include <stdlib.h>

/* ── Config ─────────────────────────────────────────── */

QuantiConfig quanti_default_config(void) {
    return (QuantiConfig){
        .max_branches     = 64,
        .prune_threshold  = 0.01,
        .initial_memory   = 256,
        .default_collapse = COLLAPSE_MAP,
        .default_backend  = BACKEND_FLOAT,
        .sc_stream_length = SC_DEFAULT_LENGTH,
        .sc_sequence_type = SC_RANDOM
    };
}

/* ── Ciclo de vida ──────────────────────────────────── */

QuantiRuntime *quanti_init(QuantiConfig config) {
    QuantiRuntime *rt = calloc(1, sizeof(QuantiRuntime));
    if (!rt) return NULL;

    rt->memory = kmem_create(config.initial_memory);
    if (!rt->memory) { free(rt); return NULL; }

    rt->branches = branch_create(config.max_branches, config.prune_threshold);
    if (!rt->branches) { kmem_free(rt->memory); free(rt); return NULL; }

    rt->pruner = pruner_default_config();
    rt->pruner.weight_threshold = config.prune_threshold;
    rt->pruner.max_active = config.max_branches;

    rt->default_collapse = config.default_collapse;
    rt->default_backend = config.default_backend;
    rt->sc_stream_length = config.sc_stream_length;
    rt->sc_sequence_type = config.sc_sequence_type;

    return rt;
}

void quanti_destroy(QuantiRuntime *rt) {
    if (!rt) return;
    kmem_free(rt->memory);
    branch_free(rt->branches);
    free(rt);
}

/* ── Creación con registro automático ───────────────── */

KaruByte quanti_false(QuantiRuntime *rt) {
    KaruByte k = karu_false();
    kmem_register(rt->memory, k);
    return k;
}

KaruByte quanti_true(QuantiRuntime *rt) {
    KaruByte k = karu_true();
    kmem_register(rt->memory, k);
    return k;
}

KaruByte quanti_superposition(QuantiRuntime *rt) {
    KaruByte k = karu_super();
    kmem_register(rt->memory, k);
    return k;
}

KaruByte quanti_undef(QuantiRuntime *rt) {
    KaruByte k = karu_undef();
    kmem_register(rt->memory, k);
    return k;
}

KaruByte quanti_prob(QuantiRuntime *rt, Distribution *dist) {
    KaruByte k = karu_prob(dist);
    if (rt->default_backend == BACKEND_SC) {
        karu_set_backend(&k, BACKEND_SC, rt->sc_stream_length);
    }
    kmem_register(rt->memory, k);
    return k;
}

KaruByte quanti_prob_sc(QuantiRuntime *rt, double p) {
    StochasticStream *stream = sc_create(p, rt->sc_stream_length, rt->sc_sequence_type);
    KaruByte k = karu_prob_sc(stream);
    kmem_register(rt->memory, k);
    return k;
}

KaruByte quanti_prob_sc_stream(QuantiRuntime *rt, StochasticStream *stream) {
    KaruByte k = karu_prob_sc(stream);
    kmem_register(rt->memory, k);
    return k;
}

/* ── Operaciones con dependencias ───────────────────── */

KaruByte quanti_and(QuantiRuntime *rt, KaruByte a, KaruByte b) {
    KaruByte result = karu_and(a, b);
    kmem_register(rt->memory, result);
    kmem_add_dependency(rt->memory, result.id, a.id);
    kmem_add_dependency(rt->memory, result.id, b.id);
    return result;
}

KaruByte quanti_or(QuantiRuntime *rt, KaruByte a, KaruByte b) {
    KaruByte result = karu_or(a, b);
    kmem_register(rt->memory, result);
    kmem_add_dependency(rt->memory, result.id, a.id);
    kmem_add_dependency(rt->memory, result.id, b.id);
    return result;
}

KaruByte quanti_not(QuantiRuntime *rt, KaruByte a) {
    KaruByte result = karu_not(a);
    kmem_register(rt->memory, result);
    kmem_add_dependency(rt->memory, result.id, a.id);
    return result;
}

/* ── Colapso ────────────────────────────────────────── */

KaruByte quanti_measure(QuantiRuntime *rt, KaruByte k, CollapseMode mode) {
    collapse_propagate(rt->memory, k.id, mode);
    KaruNode *node = kmem_find(rt->memory, k.id);
    if (node) return karu_clone(node->karu);
    return collapse_single(k, mode);
}

KaruByte quanti_measure_default(QuantiRuntime *rt, KaruByte k) {
    return quanti_measure(rt, k, rt->default_collapse);
}

/* ── Backend switching ──────────────────────────────── */

void quanti_set_backend(QuantiRuntime *rt, KaruByte *k, KaruBackend backend) {
    if (!rt || !k) return;
    karu_set_backend(k, backend, rt->sc_stream_length);
}

/* ── Branching ──────────────────────────────────────── */

size_t quanti_fork(QuantiRuntime *rt, KaruByte source) {
    return branch_fork(rt->branches, source);
}

KaruByte quanti_merge(QuantiRuntime *rt, uint32_t karu_id) {
    return branch_merge(rt->branches, karu_id);
}

/* ── Mantenimiento ──────────────────────────────────── */

size_t quanti_prune(QuantiRuntime *rt) {
    return pruner_run(rt->branches, rt->pruner);
}

/* ── Estadísticas ───────────────────────────────────── */

size_t quanti_node_count(const QuantiRuntime *rt) {
    return rt ? kmem_node_count(rt->memory) : 0;
}

size_t quanti_branch_count(const QuantiRuntime *rt) {
    return rt ? rt->branches->count : 0;
}

size_t quanti_active_branches(const QuantiRuntime *rt) {
    return rt ? branch_active_count(rt->branches) : 0;
}
