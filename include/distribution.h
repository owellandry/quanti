#ifndef QUANTI_DISTRIBUTION_H
#define QUANTI_DISTRIBUTION_H

#include <stddef.h>
#include <stdbool.h>

/*
 * Distribution — Distribuciones de probabilidad para estado P.
 *
 * Soporta tres tipos iniciales:
 *   - Normal(mean, stddev)
 *   - Discrete(probs[], labels[], n)
 *   - Uniform(min, max)
 */

typedef enum {
    DIST_NORMAL,
    DIST_DISCRETE,
    DIST_UNIFORM
} DistType;

typedef struct {
    double  *probs;     /* probabilidades (owned, length = n) */
    char   **labels;    /* etiquetas opcionales (owned, nullable, length = n) */
    size_t   n;         /* número de categorías */
} DiscreteData;

typedef struct Distribution {
    DistType type;
    union {
        struct { double mean; double stddev; }  normal;
        DiscreteData                            discrete;
        struct { double min;  double max;    }  uniform;
    } params;
} Distribution;

/* ── Creación ───────────────────────────────────────── */

Distribution *dist_normal(double mean, double stddev);
Distribution *dist_discrete(const double *probs, const char **labels, size_t n);
Distribution *dist_uniform(double min, double max);

/* ── Operaciones ────────────────────────────────────── */

double        dist_sample(const Distribution *d);        /* muestreo aleatorio */
double        dist_map_value(const Distribution *d);     /* valor más probable */
int           dist_first_index(const Distribution *d);   /* primer índice válido */

/* ── Complemento: NOT sobre distribución ────────────── */

Distribution *dist_complement(const Distribution *d);

/* ── Combinación: operaciones entre distribuciones ──── */

Distribution *dist_intersect(const Distribution *a, const Distribution *b);  /* K∩P, P·P' */
Distribution *dist_union(const Distribution *a, const Distribution *b);      /* K∪P, P+P' */

/* ── Utilidades ─────────────────────────────────────── */

Distribution *dist_clone(const Distribution *d);
void          dist_free(Distribution *d);
bool          dist_is_valid(const Distribution *d);

#endif /* QUANTI_DISTRIBUTION_H */
