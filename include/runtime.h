#ifndef QUANTI_RUNTIME_H
#define QUANTI_RUNTIME_H

#include "karubyte.h"
#include "distribution.h"
#include "memory.h"
#include "collapse.h"
#include "branch.h"
#include "pruner.h"

/*
 * Quanti Runtime — Orquestador principal.
 *
 * API de alto nivel que une todos los módulos.
 * Este es el punto de entrada para usar Quanti desde C.
 */

typedef struct {
    KaruMemory    *memory;    /* DAG de dependencias */
    BranchManager *branches;  /* manager de ramas */
    PrunerConfig   pruner;    /* configuración del pruner */
    CollapseMode   default_collapse;  /* modo de colapso por defecto */
} QuantiRuntime;

/* ── Configuración ──────────────────────────────────── */

typedef struct {
    size_t       max_branches;
    double       prune_threshold;
    size_t       initial_memory;
    CollapseMode default_collapse;
} QuantiConfig;

QuantiConfig quanti_default_config(void);

/* ── Ciclo de vida ──────────────────────────────────── */

QuantiRuntime *quanti_init(QuantiConfig config);
void           quanti_destroy(QuantiRuntime *rt);

/* ── API de alto nivel ──────────────────────────────── */

/* Crear KaruBytes y registrarlos automáticamente en el DAG */
KaruByte quanti_false(QuantiRuntime *rt);
KaruByte quanti_true(QuantiRuntime *rt);
KaruByte quanti_superposition(QuantiRuntime *rt);
KaruByte quanti_undef(QuantiRuntime *rt);
KaruByte quanti_prob(QuantiRuntime *rt, Distribution *dist);

/* Operaciones (resultado se registra en DAG con dependencias) */
KaruByte quanti_and(QuantiRuntime *rt, KaruByte a, KaruByte b);
KaruByte quanti_or(QuantiRuntime *rt, KaruByte a, KaruByte b);
KaruByte quanti_not(QuantiRuntime *rt, KaruByte a);

/* Colapso */
KaruByte quanti_measure(QuantiRuntime *rt, KaruByte k, CollapseMode mode);
KaruByte quanti_measure_default(QuantiRuntime *rt, KaruByte k);

/* Branching */
size_t quanti_fork(QuantiRuntime *rt, KaruByte source);
KaruByte quanti_merge(QuantiRuntime *rt, uint32_t karu_id);

/* Mantenimiento */
size_t quanti_prune(QuantiRuntime *rt);

/* ── Estadísticas ───────────────────────────────────── */

size_t quanti_node_count(const QuantiRuntime *rt);
size_t quanti_branch_count(const QuantiRuntime *rt);
size_t quanti_active_branches(const QuantiRuntime *rt);

#endif /* QUANTI_RUNTIME_H */
