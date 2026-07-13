#ifndef QUANTI_COLLAPSE_H
#define QUANTI_COLLAPSE_H

#include "karubyte.h"
#include "memory.h"
#include <stdbool.h>

/*
 * Collapse Engine — Motor de colapso de Quanti.
 *
 * Tres estrategias:
 *   measure:map(x)    — Máximo a Posteriori. Determinista.
 *   measure:sample(x) — Muestreo ponderado. No determinista.
 *   measure:first(x)  — Primera rama válida. Determinista por orden.
 *
 * El colapso es propagante: si x colapsa y y depende de x,
 * y también colapsa automáticamente (cascada por el DAG).
 */

typedef enum {
    COLLAPSE_MAP,      /* Máximo a Posteriori — default */
    COLLAPSE_SAMPLE,   /* Muestreo aleatorio ponderado */
    COLLAPSE_FIRST     /* Primera rama válida */
} CollapseMode;

/* ── Colapso individual ─────────────────────────────── */

/* Colapsa un KaruByte a un valor determinista (KARU_TRUE o KARU_FALSE).
 * Retorna el KaruByte colapsado. No modifica el original. */
KaruByte collapse_single(KaruByte k, CollapseMode mode);

/* ── Colapso con propagación ────────────────────────── */

/* Colapsa el nodo `karu_id` en el DAG y propaga el colapso a todos
 * los dependientes transitivos. Modifica los nodos in-place en el DAG.
 * Respeta @persistent: nodos persistentes no son colapsados por cascada.
 * Retorna el número de nodos colapsados (incluyendo el inicial). */
size_t collapse_propagate(KaruMemory *mem, uint32_t karu_id, CollapseMode mode);

/* ── Colapso forzado por presión ────────────────────── */

/* Colapsa todos los nodos no-persistentes que estén en superposición.
 * Útil cuando el runtime necesita liberar recursos.
 * Retorna el número de nodos colapsados. */
size_t collapse_all_non_persistent(KaruMemory *mem, CollapseMode mode);

/* ── Utilidades ─────────────────────────────────────── */

const char *collapse_mode_name(CollapseMode mode);

#endif /* QUANTI_COLLAPSE_H */
