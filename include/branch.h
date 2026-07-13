#ifndef QUANTI_BRANCH_H
#define QUANTI_BRANCH_H

#include "karubyte.h"
#include "memory.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Branch Manager — Fork semántico, merge y pesos.
 *
 * Cada rama es un "mundo posible" con su propio estado mutable
 * pero compartiendo datos de solo lectura con las demás.
 *
 * Cuando un KaruByte en superposición entra en un condicional,
 * el Branch Manager crea una rama por cada estado posible.
 */

/* ── Variable local de rama ─────────────────────────── */

typedef struct {
    uint32_t  karu_id;    /* ID del KaruByte */
    KaruByte  value;      /* valor en esta rama */
} BranchVar;

/* ── Rama ───────────────────────────────────────────── */

typedef struct {
    uint32_t   id;          /* ID único de la rama */
    double     weight;      /* peso (derivado de distribución, 0.0–1.0) */
    bool       active;      /* ¿rama activa o podada? */
    BranchVar *locals;      /* estado mutable local (owned) */
    size_t     local_count;
    size_t     local_cap;
} Branch;

/* ── Branch Manager ─────────────────────────────────── */

typedef struct {
    Branch    *branches;      /* array de ramas (owned) */
    size_t     count;
    size_t     capacity;
    size_t     max_branches;  /* límite configurable */
    double     prune_threshold;  /* peso mínimo para sobrevivir */
    uint32_t   next_id;
} BranchManager;

/* ── Ciclo de vida ──────────────────────────────────── */

BranchManager *branch_create(size_t max_branches, double prune_threshold);
void           branch_free(BranchManager *mgr);

/* ── Fork ───────────────────────────────────────────── */

/* Crea N ramas a partir de un KaruByte en superposición.
 * Para K: 2 ramas (FALSE, TRUE) con peso 0.5 cada una.
 * Para P(Discrete): N ramas con pesos de la distribución.
 * Retorna número de ramas creadas, o 0 si falla. */
size_t branch_fork(BranchManager *mgr, KaruByte source);

/* ── Estado local de rama ───────────────────────────── */

/* Asigna un valor a un KaruByte dentro de una rama específica */
bool branch_set_local(BranchManager *mgr, uint32_t branch_id,
                      uint32_t karu_id, KaruByte value);

/* Lee el valor de un KaruByte en una rama. Retorna NULL si no existe. */
KaruByte *branch_get_local(BranchManager *mgr, uint32_t branch_id, uint32_t karu_id);

/* ── Merge ──────────────────────────────────────────── */

/* Fusiona todas las ramas activas para un karu_id dado.
 * Si las ramas tienen valores distintos, crea superposition.
 * Retorna el KaruByte fusionado. */
KaruByte branch_merge(BranchManager *mgr, uint32_t karu_id);

/* ── Poda ───────────────────────────────────────────── */

/* Poda ramas con peso < prune_threshold.
 * Retorna número de ramas podadas. */
size_t branch_prune(BranchManager *mgr);

/* ── Consulta ───────────────────────────────────────── */

size_t  branch_active_count(const BranchManager *mgr);
Branch *branch_get(BranchManager *mgr, uint32_t branch_id);

#endif /* QUANTI_BRANCH_H */
