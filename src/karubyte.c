#include "karubyte.h"
#include "distribution.h"
#include <stdlib.h>
#include <stdio.h>

/* ── ID global auto-incremental ─────────────────────── */

static uint32_t global_id_counter = 0;

uint32_t karu_next_id(void) {
    return ++global_id_counter;
}

/* ── Creación ───────────────────────────────────────── */

KaruByte karu_false(void) {
    return (KaruByte){
        .state      = KARU_FALSE,
        .dist       = NULL,
        .id         = karu_next_id(),
        .persistent = false
    };
}

KaruByte karu_true(void) {
    return (KaruByte){
        .state      = KARU_TRUE,
        .dist       = NULL,
        .id         = karu_next_id(),
        .persistent = false
    };
}

KaruByte karu_super(void) {
    return (KaruByte){
        .state      = KARU_SUPER,
        .dist       = NULL,
        .id         = karu_next_id(),
        .persistent = false
    };
}

KaruByte karu_undef(void) {
    return (KaruByte){
        .state      = KARU_UNDEF,
        .dist       = NULL,
        .id         = karu_next_id(),
        .persistent = false
    };
}

KaruByte karu_prob(Distribution *dist) {
    return (KaruByte){
        .state      = KARU_PROB,
        .dist       = dist,   /* toma ownership */
        .id         = karu_next_id(),
        .persistent = false
    };
}

/* ── Álgebra AND ────────────────────────────────────── *
 *
 * Tabla de verdad (de la spec):
 *
 *   AND │  0    1    K    Ø    P
 *   ────┼──────────────────────
 *    0  │  0    0    0    0    0
 *    1  │  0    1    K    Ø    P
 *    K  │  0    K    K    Ø   K∩P
 *    Ø  │  0    Ø    Ø    Ø    Ø
 *    P  │  0    P   K∩P   Ø   P·P'
 */

KaruByte karu_and(KaruByte a, KaruByte b) {
    /* 0 AND anything = 0 (absorción) */
    if (a.state == KARU_FALSE || b.state == KARU_FALSE)
        return karu_false();

    /* Ø AND anything (except 0) = Ø */
    if (a.state == KARU_UNDEF || b.state == KARU_UNDEF)
        return karu_undef();

    /* 1 AND x = x */
    if (a.state == KARU_TRUE) return karu_clone(b);
    if (b.state == KARU_TRUE) return karu_clone(a);

    /* K AND K = K */
    if (a.state == KARU_SUPER && b.state == KARU_SUPER)
        return karu_super();

    /* K AND P = K∩P → resultado probabilístico con intersección */
    if ((a.state == KARU_SUPER && b.state == KARU_PROB) ||
        (a.state == KARU_PROB  && b.state == KARU_SUPER)) {
        KaruByte prob_side = (a.state == KARU_PROB) ? a : b;
        return karu_prob(dist_clone(prob_side.dist));
    }

    /* P AND P = P·P' → intersección de distribuciones */
    if (a.state == KARU_PROB && b.state == KARU_PROB) {
        Distribution *combined = dist_intersect(a.dist, b.dist);
        return karu_prob(combined);
    }

    /* Fallback (no debería llegar aquí) */
    return karu_undef();
}

/* ── Álgebra OR ─────────────────────────────────────── *
 *
 *   OR  │  0    1    K    Ø    P
 *   ────┼──────────────────────
 *    0  │  0    1    K    Ø    P
 *    1  │  1    1    1    1    1
 *    K  │  K    1    K   K∪Ø  K∪P
 *    Ø  │  Ø    1   K∪Ø   Ø   Ø∪P
 *    P  │  P    1   K∪P  Ø∪P  P+P'
 */

KaruByte karu_or(KaruByte a, KaruByte b) {
    /* 1 OR anything = 1 (absorción) */
    if (a.state == KARU_TRUE || b.state == KARU_TRUE)
        return karu_true();

    /* 0 OR x = x */
    if (a.state == KARU_FALSE) return karu_clone(b);
    if (b.state == KARU_FALSE) return karu_clone(a);

    /* K OR K = K */
    if (a.state == KARU_SUPER && b.state == KARU_SUPER)
        return karu_super();

    /* Ø OR Ø = Ø */
    if (a.state == KARU_UNDEF && b.state == KARU_UNDEF)
        return karu_undef();

    /* K OR Ø = K∪Ø → superposición domina */
    if ((a.state == KARU_SUPER && b.state == KARU_UNDEF) ||
        (a.state == KARU_UNDEF && b.state == KARU_SUPER))
        return karu_super();

    /* K OR P = K∪P → unión */
    if ((a.state == KARU_SUPER && b.state == KARU_PROB) ||
        (a.state == KARU_PROB  && b.state == KARU_SUPER)) {
        KaruByte prob_side = (a.state == KARU_PROB) ? a : b;
        return karu_prob(dist_clone(prob_side.dist));
    }

    /* Ø OR P = Ø∪P → probabilístico con contexto indefinido */
    if ((a.state == KARU_UNDEF && b.state == KARU_PROB) ||
        (a.state == KARU_PROB  && b.state == KARU_UNDEF)) {
        KaruByte prob_side = (a.state == KARU_PROB) ? a : b;
        return karu_prob(dist_clone(prob_side.dist));
    }

    /* P OR P = P+P' → unión de distribuciones */
    if (a.state == KARU_PROB && b.state == KARU_PROB) {
        Distribution *combined = dist_union(a.dist, b.dist);
        return karu_prob(combined);
    }

    return karu_undef();
}

/* ── Álgebra NOT ────────────────────────────────────── *
 *
 *   NOT 0    = 1
 *   NOT 1    = 0
 *   NOT K    = K   (inversión en ambas ramas)
 *   NOT Ø    = Ø
 *   NOT P(d) = P(1-d)
 */

KaruByte karu_not(KaruByte a) {
    switch (a.state) {
    case KARU_FALSE: return karu_true();
    case KARU_TRUE:  return karu_false();
    case KARU_SUPER: return karu_super();
    case KARU_UNDEF: return karu_undef();
    case KARU_PROB:  return karu_prob(dist_complement(a.dist));
    }
    return karu_undef();
}

/* ── Inspección ─────────────────────────────────────── */

bool karu_is_deterministic(KaruByte k) {
    return k.state == KARU_FALSE || k.state == KARU_TRUE;
}

bool karu_is_superposition(KaruByte k) {
    return k.state == KARU_SUPER;
}

bool karu_is_undefined(KaruByte k) {
    return k.state == KARU_UNDEF;
}

bool karu_is_probabilistic(KaruByte k) {
    return k.state == KARU_PROB;
}

const char *karu_state_name(KaruState s) {
    switch (s) {
    case KARU_FALSE: return "0 (false)";
    case KARU_TRUE:  return "1 (true)";
    case KARU_SUPER: return "K (superposition)";
    case KARU_UNDEF: return "O/ (undefined)";
    case KARU_PROB:  return "P (probabilistic)";
    }
    return "unknown";
}

/* ── Utilidades ─────────────────────────────────────── */

KaruByte karu_clone(KaruByte k) {
    KaruByte copy;
    copy.state      = k.state;
    copy.dist       = k.dist ? dist_clone(k.dist) : NULL;
    copy.id         = karu_next_id();
    copy.persistent = k.persistent;
    return copy;
}

void karu_free(KaruByte *k) {
    if (!k) return;
    if (k->dist) {
        dist_free(k->dist);
        k->dist = NULL;
    }
}
