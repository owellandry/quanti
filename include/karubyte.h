#ifndef QUANTI_KARUBYTE_H
#define QUANTI_KARUBYTE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * KaruByte — Unidad lógica fundamental de Quanti.
 *
 * Un KaruByte es un valor único multiestado que puede encontrarse en:
 *   0  — falso determinista
 *   1  — verdadero determinista
 *   K  — superposición (ambos activos simultáneamente)
 *   Ø  — indefinido (estado desconocido)
 *   P  — probabilístico (distribución asociada)
 */

/* ── Estados ────────────────────────────────────────── */

typedef enum {
    KARU_FALSE = 0,   /* 0 — determinista falso       */
    KARU_TRUE  = 1,   /* 1 — determinista verdadero    */
    KARU_SUPER = 2,   /* K — superposición             */
    KARU_UNDEF = 3,   /* Ø — indefinido                */
    KARU_PROB  = 4    /* P — probabilístico            */
} KaruState;

/* ── Forward declarations ───────────────────────────── */

typedef struct Distribution Distribution;  /* defined in distribution.h */

/* ── KaruByte ───────────────────────────────────────── */

typedef struct {
    KaruState state;

    /* Solo relevante cuando state == KARU_PROB.
     * Puntero a distribución asociada (owned, nullable). */
    Distribution *dist;

    /* ID único para tracking en el DAG de dependencias */
    uint32_t id;

    /* Flags */
    bool persistent;   /* @persistent — resiste colapso automático */
} KaruByte;

/* ── Creación ───────────────────────────────────────── */

KaruByte karu_false(void);
KaruByte karu_true(void);
KaruByte karu_super(void);
KaruByte karu_undef(void);
KaruByte karu_prob(Distribution *dist);   /* toma ownership del dist */

/* ── Álgebra ────────────────────────────────────────── */

KaruByte karu_and(KaruByte a, KaruByte b);
KaruByte karu_or(KaruByte a, KaruByte b);
KaruByte karu_not(KaruByte a);

/* ── Inspección ─────────────────────────────────────── */

bool        karu_is_deterministic(KaruByte k);
bool        karu_is_superposition(KaruByte k);
bool        karu_is_undefined(KaruByte k);
bool        karu_is_probabilistic(KaruByte k);
const char *karu_state_name(KaruState s);

/* ── Utilidades ─────────────────────────────────────── */

KaruByte karu_clone(KaruByte k);
void     karu_free(KaruByte *k);   /* libera distribución si existe */

/* ── ID global auto-incremental ─────────────────────── */

uint32_t karu_next_id(void);

#endif /* QUANTI_KARUBYTE_H */
