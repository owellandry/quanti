#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "../include/runtime.h"
#include "../include/stochastic.h"

#define N_OPS 10000
#define STREAM_LEN 1024

static double time_diff_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

void bench_float_backend(void) {
    printf("\n=== Backend FLOAT ===\n");

    QuantiConfig cfg = quanti_default_config();
    cfg.default_backend = BACKEND_FLOAT;
    QuantiRuntime *rt = quanti_init(cfg);

    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    KaruByte *karus = malloc(N_OPS * sizeof(KaruByte));
    for (int i = 0; i < N_OPS; i++) {
        double p = (double)(i % 100) / 100.0;
        Distribution *d = dist_discrete((double[]){p, 1.0 - p}, NULL, 2);
        karus[i] = quanti_prob(rt, d);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("  Create %d KaruBytes:  %.3f ms\n", N_OPS, time_diff_ms(t0, t1));

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N_OPS - 1; i += 2) {
        quanti_and(rt, karus[i], karus[i + 1]);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("  %d AND operations:    %.3f ms\n", N_OPS / 2, time_diff_ms(t0, t1));

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N_OPS - 1; i += 2) {
        quanti_or(rt, karus[i], karus[i + 1]);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("  %d OR operations:     %.3f ms\n", N_OPS / 2, time_diff_ms(t0, t1));

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N_OPS; i++) {
        quanti_not(rt, karus[i]);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("  %d NOT operations:    %.3f ms\n", N_OPS, time_diff_ms(t0, t1));

    free(karus);
    quanti_destroy(rt);
}

void bench_sc_backend(void) {
    printf("\n=== Backend SC (stream=%d) ===\n", STREAM_LEN);

    QuantiConfig cfg = quanti_default_config();
    cfg.default_backend = BACKEND_SC;
    cfg.sc_stream_length = STREAM_LEN;
    QuantiRuntime *rt = quanti_init(cfg);

    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    KaruByte *karus = malloc(N_OPS * sizeof(KaruByte));
    for (int i = 0; i < N_OPS; i++) {
        double p = (double)(i % 100) / 100.0;
        karus[i] = quanti_prob_sc(rt, p);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("  Create %d KaruBytes:  %.3f ms\n", N_OPS, time_diff_ms(t0, t1));

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N_OPS - 1; i += 2) {
        quanti_and(rt, karus[i], karus[i + 1]);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("  %d AND operations:    %.3f ms\n", N_OPS / 2, time_diff_ms(t0, t1));

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N_OPS - 1; i += 2) {
        quanti_or(rt, karus[i], karus[i + 1]);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("  %d OR operations:     %.3f ms\n", N_OPS / 2, time_diff_ms(t0, t1));

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N_OPS; i++) {
        quanti_not(rt, karus[i]);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("  %d NOT operations:    %.3f ms\n", N_OPS, time_diff_ms(t0, t1));

    free(karus);
    quanti_destroy(rt);
}

void bench_accuracy(void) {
    printf("\n=== Accuracy comparison ===\n");
    printf("  NOTE: FLOAT uses Bayesian intersection (renormalized)\n");
    printf("        SC uses raw probabilistic AND (P(A)*P(B))\n\n");

    double test_cases[][2] = {
        {0.7, 0.8},
        {0.3, 0.4},
        {0.5, 0.5},
        {0.9, 0.1},
        {0.6, 0.6}
    };
    int n_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int c = 0; c < n_cases; c++) {
        double pa = test_cases[c][0];
        double pb = test_cases[c][1];

        Distribution *da = dist_discrete((double[]){pa, 1.0 - pa}, NULL, 2);
        Distribution *db = dist_discrete((double[]){pb, 1.0 - pb}, NULL, 2);
        Distribution *float_and = dist_intersect(da, db);
        double float_result = float_and->params.discrete.probs[0];

        double raw_and = pa * pb;
        double raw_not = (1.0 - pa) * (1.0 - pb);
        double bayesian_expected = raw_and / (raw_and + raw_not);

        StochasticStream *sa = sc_create(pa, STREAM_LEN, SC_RANDOM);
        StochasticStream *sb = sc_create(pb, STREAM_LEN, SC_RANDOM);
        StochasticStream *sc_result = sc_and(sa, sb);
        double sc_est = sc_estimate(sc_result);

        double float_err = fabs(float_result - bayesian_expected);
        double sc_err = fabs(sc_est - raw_and);

        printf("  AND(%.1f, %.1f):\n", pa, pb);
        printf("    Float: %.4f (bayesian expected=%.4f, err=%.6f)\n",
               float_result, bayesian_expected, float_err);
        printf("    SC:    %.4f (raw expected=%.4f, err=%.6f)\n",
               sc_est, raw_and, sc_err);

        dist_free(da); dist_free(db); dist_free(float_and);
        sc_free(sa); sc_free(sb); sc_free(sc_result);
    }
}

int main(void) {
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║    Quanti SC Benchmark — Float vs Stochastic     ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");

    bench_float_backend();
    bench_sc_backend();
    bench_accuracy();

    printf("\nDone.\n");
    return 0;
}
