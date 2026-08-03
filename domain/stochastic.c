#include "stochastic.h"
#include "distribution.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static int sc_rng_seeded = 0;
static uint32_t sc_stream_counter = 0;

static void sc_ensure_seeded(void) {
    if (!sc_rng_seeded) {
        srand((unsigned)time(NULL));
        sc_rng_seeded = 1;
    }
}

static double van_der_corput(uint32_t n, uint32_t base) {
    double result = 0.0;
    double fraction = 1.0 / base;
    while (n > 0) {
        result += (n % base) * fraction;
        n /= base;
        fraction /= base;
    }
    return result;
}

static uint32_t lfsr_next(uint32_t *state) {
    uint32_t bit = *state & 1;
    *state = (*state >> 1) ^ (-(int32_t)bit & 0xD0000001u);
    return *state;
}

static bool generate_bit(double p, SCSequenceType type, uint32_t *seed_state) {
    double r;
    switch (type) {
    case SC_VAN_DER_CORPUT:
        r = van_der_corput((*seed_state)++, 2);
        break;
    case SC_LFSR:
        r = (double)lfsr_next(seed_state) / (double)UINT32_MAX;
        break;
    case SC_RANDOM:
    default:
        *seed_state = *seed_state * 1103515245u + 12345u;
        r = (double)((*seed_state >> 16) & 0x7FFF) / 32768.0;
        break;
    }
    return r < p;
}

static void sc_recount(StochasticStream *sc) {
    size_t ones = 0;
    size_t byte_len = (sc->length + 7) / 8;
    for (size_t i = 0; i < byte_len; i++) {
        uint8_t b = sc->bits[i];
        while (b) {
            ones += b & 1;
            b >>= 1;
        }
    }
    sc->ones_count = ones;
    sc->estimated_p = sc->length > 0 ? (double)ones / (double)sc->length : 0.0;
}

StochasticStream *sc_create_empty(size_t length, SCSequenceType type) {
    sc_ensure_seeded();
    StochasticStream *sc = calloc(1, sizeof(StochasticStream));
    if (!sc) return NULL;

    size_t byte_len = (length + 7) / 8;
    sc->bits = calloc(byte_len, sizeof(uint8_t));
    if (!sc->bits) { free(sc); return NULL; }

    sc->length = length;
    sc->position = 0;
    sc->type = type;
    sc->seed = (uint32_t)rand() ^ (sc_stream_counter++ * 2654435761u);
    sc->ones_count = 0;
    sc->estimated_p = 0.0;
    return sc;
}

StochasticStream *sc_create(double p, size_t length, SCSequenceType type) {
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;

    StochasticStream *sc = sc_create_empty(length, type);
    if (!sc) return NULL;

    uint32_t state = sc->seed;
    for (size_t i = 0; i < length; i++) {
        if (generate_bit(p, type, &state)) {
            sc->bits[i / 8] |= (1 << (i % 8));
            sc->ones_count++;
        }
    }
    sc->estimated_p = (double)sc->ones_count / (double)length;
    return sc;
}

StochasticStream *sc_from_distribution(const Distribution *d, size_t length) {
    if (!d) return sc_create(0.0, length, SC_VAN_DER_CORPUT);

    double p;
    switch (d->type) {
    case DIST_DISCRETE:
        if (d->params.discrete.n >= 2) {
            p = d->params.discrete.probs[0];
        } else {
            p = d->params.discrete.probs[0];
        }
        break;
    case DIST_NORMAL:
        p = 0.5 * (1.0 + erf(d->params.normal.mean / (d->params.normal.stddev * sqrt(2.0))));
        break;
    case DIST_UNIFORM:
        p = (d->params.uniform.min + d->params.uniform.max) / 2.0;
        if (p < 0.0) p = 0.0;
        if (p > 1.0) p = 1.0;
        break;
    default:
        p = 0.5;
        break;
    }

    return sc_create(p, length, SC_VAN_DER_CORPUT);
}

StochasticStream *sc_and(const StochasticStream *a, const StochasticStream *b) {
    if (!a || !b) return NULL;

    size_t len = a->length < b->length ? a->length : b->length;
    StochasticStream *result = sc_create_empty(len, a->type);
    if (!result) return NULL;

    size_t byte_len = (len + 7) / 8;
    for (size_t i = 0; i < byte_len; i++) {
        uint8_t mask = (i == byte_len - 1 && len % 8 != 0)
                       ? (uint8_t)((1 << (len % 8)) - 1)
                       : 0xFF;
        result->bits[i] = (a->bits[i] & b->bits[i]) & mask;
    }

    sc_recount(result);
    return result;
}

StochasticStream *sc_or(const StochasticStream *a, const StochasticStream *b) {
    if (!a || !b) return NULL;

    size_t len = a->length < b->length ? a->length : b->length;
    StochasticStream *result = sc_create_empty(len, a->type);
    if (!result) return NULL;

    const StochasticStream *sa = a;
    const StochasticStream *sb = b;

    size_t byte_len = (len + 7) / 8;
    for (size_t i = 0; i < byte_len; i++) {
        uint8_t mask = (i == byte_len - 1 && len % 8 != 0)
                       ? (uint8_t)((1 << (len % 8)) - 1)
                       : 0xFF;
        result->bits[i] = (sa->bits[i] | sb->bits[i]) & mask;
    }

    sc_recount(result);
    return result;
}

StochasticStream *sc_not(const StochasticStream *a) {
    if (!a) return NULL;

    StochasticStream *result = sc_create_empty(a->length, a->type);
    if (!result) return NULL;

    size_t byte_len = (a->length + 7) / 8;
    for (size_t i = 0; i < byte_len; i++) {
        result->bits[i] = ~a->bits[i];
    }

    size_t remainder = a->length % 8;
    if (remainder != 0) {
        uint8_t mask = (uint8_t)((1 << remainder) - 1);
        result->bits[byte_len - 1] &= mask;
    }

    sc_recount(result);
    return result;
}

double sc_estimate(const StochasticStream *sc) {
    if (!sc || sc->length == 0) return 0.0;
    return sc->estimated_p;
}

double sc_estimate_window(const StochasticStream *sc, size_t window) {
    if (!sc || window == 0 || window > sc->length) return 0.0;

    size_t ones = 0;
    for (size_t i = 0; i < window; i++) {
        if (sc->bits[i / 8] & (1 << (i % 8))) {
            ones++;
        }
    }
    return (double)ones / (double)window;
}

bool sc_converged(const StochasticStream *sc, double threshold) {
    if (!sc || sc->length == 0) return false;

    double p = sc->estimated_p;
    double se = sqrt(p * (1.0 - p) / (double)sc->length);
    return se < threshold;
}

double sc_estimate_converging(StochasticStream *sc, double confidence, size_t max_window) {
    if (!sc) return 0.0;

    double target_se = (1.0 - confidence) / 2.0;
    size_t window = 64;

    while (window <= sc->length && window <= max_window) {
        double p = sc_estimate_window(sc, window);
        double se = sqrt(p * (1.0 - p) / (double)window);
        if (se < target_se) {
            return p;
        }
        window *= 2;
    }

    return sc_estimate(sc);
}

struct Distribution *sc_to_distribution(const StochasticStream *sc) {
    if (!sc) return dist_discrete((double[]){0.5, 0.5}, NULL, 2);

    double p = sc_estimate(sc);

    double probs[2] = {p, 1.0 - p};
    return dist_discrete(probs, NULL, 2);
}

StochasticStream *sc_clone(const StochasticStream *sc) {
    if (!sc) return NULL;

    StochasticStream *copy = calloc(1, sizeof(StochasticStream));
    if (!copy) return NULL;

    size_t byte_len = (sc->length + 7) / 8;
    copy->bits = malloc(byte_len);
    if (!copy->bits) { free(copy); return NULL; }

    memcpy(copy->bits, sc->bits, byte_len);
    copy->length = sc->length;
    copy->position = sc->position;
    copy->estimated_p = sc->estimated_p;
    copy->ones_count = sc->ones_count;
    copy->type = sc->type;
    copy->seed = sc->seed;
    return copy;
}

void sc_free(StochasticStream *sc) {
    if (!sc) return;
    if (sc->bits) {
        free(sc->bits);
        sc->bits = NULL;
    }
    free(sc);
}
