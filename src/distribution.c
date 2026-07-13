#include "distribution.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

/* ── RNG simple (reemplazable por mejor PRNG después) ── */

static int rng_seeded = 0;

static void ensure_seeded(void) {
    if (!rng_seeded) {
        srand((unsigned)time(NULL));
        rng_seeded = 1;
    }
}

/* Uniforme [0, 1) */
static double rand_uniform01(void) {
    ensure_seeded();
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

/* Box-Muller para distribución normal */
static double rand_normal(double mean, double stddev) {
    double u1 = rand_uniform01();
    double u2 = rand_uniform01();
    /* Evitar log(0) */
    if (u1 < 1e-15) u1 = 1e-15;
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2);
    return mean + stddev * z;
}

/* ── Creación ───────────────────────────────────────── */

Distribution *dist_normal(double mean, double stddev) {
    Distribution *d = calloc(1, sizeof(Distribution));
    if (!d) return NULL;
    d->type = DIST_NORMAL;
    d->params.normal.mean   = mean;
    d->params.normal.stddev = stddev;
    return d;
}

Distribution *dist_discrete(const double *probs, const char **labels, size_t n) {
    if (n == 0 || !probs) return NULL;

    Distribution *d = calloc(1, sizeof(Distribution));
    if (!d) return NULL;

    d->type = DIST_DISCRETE;
    d->params.discrete.n = n;

    d->params.discrete.probs = malloc(n * sizeof(double));
    if (!d->params.discrete.probs) { free(d); return NULL; }
    memcpy(d->params.discrete.probs, probs, n * sizeof(double));

    if (labels) {
        d->params.discrete.labels = malloc(n * sizeof(char *));
        if (!d->params.discrete.labels) {
            free(d->params.discrete.probs);
            free(d);
            return NULL;
        }
        for (size_t i = 0; i < n; i++) {
            d->params.discrete.labels[i] = labels[i] ? strdup(labels[i]) : NULL;
        }
    } else {
        d->params.discrete.labels = NULL;
    }

    return d;
}

Distribution *dist_uniform(double min, double max) {
    Distribution *d = calloc(1, sizeof(Distribution));
    if (!d) return NULL;
    d->type = DIST_UNIFORM;
    d->params.uniform.min = min;
    d->params.uniform.max = max;
    return d;
}

/* ── Muestreo ───────────────────────────────────────── */

double dist_sample(const Distribution *d) {
    if (!d) return 0.0;

    switch (d->type) {
    case DIST_NORMAL:
        return rand_normal(d->params.normal.mean, d->params.normal.stddev);

    case DIST_DISCRETE: {
        double r = rand_uniform01();
        double cumulative = 0.0;
        for (size_t i = 0; i < d->params.discrete.n; i++) {
            cumulative += d->params.discrete.probs[i];
            if (r < cumulative) return (double)i;
        }
        return (double)(d->params.discrete.n - 1);
    }

    case DIST_UNIFORM:
        return d->params.uniform.min +
               rand_uniform01() * (d->params.uniform.max - d->params.uniform.min);
    }

    return 0.0;
}

/* ── MAP (valor más probable) ───────────────────────── */

double dist_map_value(const Distribution *d) {
    if (!d) return 0.0;

    switch (d->type) {
    case DIST_NORMAL:
        return d->params.normal.mean;   /* la moda de una normal es la media */

    case DIST_DISCRETE: {
        size_t best = 0;
        for (size_t i = 1; i < d->params.discrete.n; i++) {
            if (d->params.discrete.probs[i] > d->params.discrete.probs[best])
                best = i;
        }
        return (double)best;
    }

    case DIST_UNIFORM:
        /* Uniforme no tiene moda, devolvemos el punto medio */
        return (d->params.uniform.min + d->params.uniform.max) / 2.0;
    }

    return 0.0;
}

/* ── First index ────────────────────────────────────── */

int dist_first_index(const Distribution *d) {
    if (!d) return 0;

    switch (d->type) {
    case DIST_DISCRETE:
        for (size_t i = 0; i < d->params.discrete.n; i++) {
            if (d->params.discrete.probs[i] > 0.0) return (int)i;
        }
        return 0;

    case DIST_NORMAL:
    case DIST_UNIFORM:
        return 0;  /* no aplica, devolver 0 */
    }

    return 0;
}

/* ── Complemento: P(d) → P(1-d) ────────────────────── */

Distribution *dist_complement(const Distribution *d) {
    if (!d) return NULL;

    switch (d->type) {
    case DIST_NORMAL:
        /* Complemento de normal: negamos la media */
        return dist_normal(-d->params.normal.mean, d->params.normal.stddev);

    case DIST_DISCRETE: {
        double *comp_probs = malloc(d->params.discrete.n * sizeof(double));
        if (!comp_probs) return NULL;

        /* Invertir probabilidades: normalizar (1-p_i) */
        double sum = 0.0;
        for (size_t i = 0; i < d->params.discrete.n; i++) {
            comp_probs[i] = 1.0 - d->params.discrete.probs[i];
            if (comp_probs[i] < 0.0) comp_probs[i] = 0.0;
            sum += comp_probs[i];
        }
        if (sum > 0.0) {
            for (size_t i = 0; i < d->params.discrete.n; i++)
                comp_probs[i] /= sum;
        }

        Distribution *result = dist_discrete(
            comp_probs,
            (const char **)d->params.discrete.labels,
            d->params.discrete.n
        );
        free(comp_probs);
        return result;
    }

    case DIST_UNIFORM:
        /* Complemento: invertir el rango */
        return dist_uniform(-d->params.uniform.max, -d->params.uniform.min);
    }

    return NULL;
}

/* ── Intersección (K∩P, P·P') ───────────────────────── */

Distribution *dist_intersect(const Distribution *a, const Distribution *b) {
    if (!a || !b) return NULL;

    /* Caso discreto × discreto: multiplicar probabilidades y renormalizar */
    if (a->type == DIST_DISCRETE && b->type == DIST_DISCRETE) {
        size_t n = (a->params.discrete.n < b->params.discrete.n)
                   ? a->params.discrete.n : b->params.discrete.n;

        double *probs = malloc(n * sizeof(double));
        if (!probs) return NULL;

        double sum = 0.0;
        for (size_t i = 0; i < n; i++) {
            probs[i] = a->params.discrete.probs[i] * b->params.discrete.probs[i];
            sum += probs[i];
        }
        if (sum > 0.0) {
            for (size_t i = 0; i < n; i++) probs[i] /= sum;
        }

        Distribution *result = dist_discrete(
            probs,
            (const char **)a->params.discrete.labels,
            n
        );
        free(probs);
        return result;
    }

    /* Caso normal × normal: producto de gaussianas */
    if (a->type == DIST_NORMAL && b->type == DIST_NORMAL) {
        double var_a = a->params.normal.stddev * a->params.normal.stddev;
        double var_b = b->params.normal.stddev * b->params.normal.stddev;
        double new_var = 1.0 / (1.0/var_a + 1.0/var_b);
        double new_mean = new_var * (a->params.normal.mean/var_a + b->params.normal.mean/var_b);
        return dist_normal(new_mean, sqrt(new_var));
    }

    /* Fallback: clonar la primera */
    return dist_clone(a);
}

/* ── Unión (K∪P, P+P') ─────────────────────────────── */

Distribution *dist_union(const Distribution *a, const Distribution *b) {
    if (!a || !b) return NULL;

    /* Caso discreto + discreto: promediar probabilidades */
    if (a->type == DIST_DISCRETE && b->type == DIST_DISCRETE) {
        size_t n = (a->params.discrete.n > b->params.discrete.n)
                   ? a->params.discrete.n : b->params.discrete.n;

        double *probs = calloc(n, sizeof(double));
        if (!probs) return NULL;

        for (size_t i = 0; i < n; i++) {
            double pa = (i < a->params.discrete.n) ? a->params.discrete.probs[i] : 0.0;
            double pb = (i < b->params.discrete.n) ? b->params.discrete.probs[i] : 0.0;
            probs[i] = (pa + pb) / 2.0;
        }

        Distribution *result = dist_discrete(probs, NULL, n);
        free(probs);
        return result;
    }

    /* Caso normal + normal: mezcla (mixture) simplificada */
    if (a->type == DIST_NORMAL && b->type == DIST_NORMAL) {
        double new_mean = (a->params.normal.mean + b->params.normal.mean) / 2.0;
        double new_std  = sqrt(
            (a->params.normal.stddev * a->params.normal.stddev +
             b->params.normal.stddev * b->params.normal.stddev) / 2.0
        );
        return dist_normal(new_mean, new_std);
    }

    return dist_clone(a);
}

/* ── Clone / Free ───────────────────────────────────── */

Distribution *dist_clone(const Distribution *d) {
    if (!d) return NULL;

    switch (d->type) {
    case DIST_NORMAL:
        return dist_normal(d->params.normal.mean, d->params.normal.stddev);

    case DIST_DISCRETE:
        return dist_discrete(
            d->params.discrete.probs,
            (const char **)d->params.discrete.labels,
            d->params.discrete.n
        );

    case DIST_UNIFORM:
        return dist_uniform(d->params.uniform.min, d->params.uniform.max);
    }

    return NULL;
}

void dist_free(Distribution *d) {
    if (!d) return;

    if (d->type == DIST_DISCRETE) {
        if (d->params.discrete.labels) {
            for (size_t i = 0; i < d->params.discrete.n; i++)
                free(d->params.discrete.labels[i]);
            free(d->params.discrete.labels);
        }
        free(d->params.discrete.probs);
    }

    free(d);
}

bool dist_is_valid(const Distribution *d) {
    if (!d) return false;

    switch (d->type) {
    case DIST_NORMAL:
        return d->params.normal.stddev > 0.0;

    case DIST_DISCRETE: {
        if (d->params.discrete.n == 0 || !d->params.discrete.probs) return false;
        double sum = 0.0;
        for (size_t i = 0; i < d->params.discrete.n; i++) {
            if (d->params.discrete.probs[i] < 0.0) return false;
            sum += d->params.discrete.probs[i];
        }
        return fabs(sum - 1.0) < 1e-6;
    }

    case DIST_UNIFORM:
        return d->params.uniform.max > d->params.uniform.min;
    }

    return false;
}
