#ifndef QUANTI_STOCHASTIC_H
#define QUANTI_STOCHASTIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SC_DEFAULT_LENGTH 1024

typedef enum {
    SC_RANDOM,
    SC_VAN_DER_CORPUT,
    SC_LFSR
} SCSequenceType;

typedef struct StochasticStream {
    uint8_t   *bits;
    size_t     length;
    size_t     position;
    double     estimated_p;
    size_t     ones_count;
    SCSequenceType type;
    uint32_t   seed;
} StochasticStream;

StochasticStream *sc_create(double p, size_t length, SCSequenceType type);
StochasticStream *sc_create_empty(size_t length, SCSequenceType type);

struct Distribution;
StochasticStream *sc_from_distribution(const struct Distribution *d, size_t length);

struct KaruByte;
StochasticStream *sc_from_karu(const struct KaruByte *k, size_t length);

StochasticStream *sc_and(const StochasticStream *a, const StochasticStream *b);
StochasticStream *sc_or(const StochasticStream *a, const StochasticStream *b);
StochasticStream *sc_not(const StochasticStream *a);

double sc_estimate(const StochasticStream *sc);
double sc_estimate_window(const StochasticStream *sc, size_t window);
bool   sc_converged(const StochasticStream *sc, double threshold);
double sc_estimate_converging(StochasticStream *sc, double confidence, size_t max_window);

struct Distribution *sc_to_distribution(const StochasticStream *sc);

void sc_free(StochasticStream *sc);
StochasticStream *sc_clone(const StochasticStream *sc);

#endif
