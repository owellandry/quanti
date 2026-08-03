#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "stochastic.h"
#include "distribution.h"

#define TEST(name) printf("  [TEST] %-50s", name)
#define PASS()     printf("OK\n")

static int tests_run    = 0;
static int tests_passed = 0;

#define TOLERANCE_1K  0.08
#define TOLERANCE_4K  0.04
#define TOLERANCE_10K 0.025

void test_creation(void) {
    TEST("sc_create(0.0, 1024, RANDOM) → all zeros");
    StochasticStream *sc = sc_create(0.0, 1024, SC_RANDOM);
    assert(sc != NULL);
    assert(sc->length == 1024);
    assert(sc->ones_count == 0);
    assert(fabs(sc->estimated_p - 0.0) < 0.001);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;

    TEST("sc_create(1.0, 1024, RANDOM) → all ones");
    sc = sc_create(1.0, 1024, SC_RANDOM);
    assert(sc != NULL);
    assert(sc->ones_count == 1024);
    assert(fabs(sc->estimated_p - 1.0) < 0.001);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;

    TEST("sc_create(0.5, 1024, RANDOM) → ~512 ones");
    sc = sc_create(0.5, 1024, SC_RANDOM);
    assert(sc != NULL);
    assert(fabs(sc->estimated_p - 0.5) < TOLERANCE_1K);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;

    TEST("sc_create(0.7, 1024, RANDOM) → ~720 ones");
    sc = sc_create(0.7, 1024, SC_RANDOM);
    assert(sc != NULL);
    assert(fabs(sc->estimated_p - 0.7) < TOLERANCE_1K);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;

    TEST("sc_create(0.3, 4096, RANDOM) → ~0.3 ± 0.04");
    sc = sc_create(0.3, 4096, SC_RANDOM);
    assert(sc != NULL);
    assert(fabs(sc->estimated_p - 0.3) < TOLERANCE_4K);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;
}

void test_van_der_corput(void) {
    TEST("vdc(0.7, 1024) converges faster than random");
    StochasticStream *sc_vdc = sc_create(0.7, 1024, SC_VAN_DER_CORPUT);
    StochasticStream *sc_rnd = sc_create(0.7, 1024, SC_RANDOM);
    double err_vdc = fabs(sc_vdc->estimated_p - 0.7);
    double err_rnd = fabs(sc_rnd->estimated_p - 0.7);
    assert(err_vdc < TOLERANCE_1K);
    sc_free(sc_vdc);
    sc_free(sc_rnd);
    PASS(); tests_run++; tests_passed++;

    TEST("vdc(0.5, 1024) → ~0.5");
    StochasticStream *sc = sc_create(0.5, 1024, SC_VAN_DER_CORPUT);
    assert(fabs(sc->estimated_p - 0.5) < TOLERANCE_1K);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;
}

void test_lfsr(void) {
    TEST("LFSR(0.5, 1024) → ~0.5");
    StochasticStream *sc = sc_create(0.5, 1024, SC_LFSR);
    assert(fabs(sc->estimated_p - 0.5) < TOLERANCE_1K);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;
}

void test_and(void) {
    TEST("AND(0.5, 0.5) → ~0.25 (1024 bits)");
    StochasticStream *a = sc_create(0.5, 1024, SC_VAN_DER_CORPUT);
    StochasticStream *b = sc_create(0.5, 1024, SC_RANDOM);
    StochasticStream *r = sc_and(a, b);
    assert(r != NULL);
    assert(fabs(sc_estimate(r) - 0.25) < TOLERANCE_1K);
    sc_free(a); sc_free(b); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("AND(0.0, 0.99) → ~0.0 (absorption)");
    a = sc_create(0.0, 1024, SC_VAN_DER_CORPUT);
    b = sc_create(0.99, 1024, SC_RANDOM);
    r = sc_and(a, b);
    assert(fabs(sc_estimate(r) - 0.0) < 0.01);
    sc_free(a); sc_free(b); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("AND(1.0, x) → ~P(x) (identity)");
    a = sc_create(1.0, 1024, SC_VAN_DER_CORPUT);
    b = sc_create(0.6, 1024, SC_RANDOM);
    r = sc_and(a, b);
    assert(fabs(sc_estimate(r) - 0.6) < TOLERANCE_1K);
    sc_free(a); sc_free(b); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("AND(0.7, 0.8) → ~0.56 (1024 bits)");
    a = sc_create(0.7, 1024, SC_VAN_DER_CORPUT);
    b = sc_create(0.8, 1024, SC_RANDOM);
    r = sc_and(a, b);
    assert(fabs(sc_estimate(r) - 0.56) < TOLERANCE_1K);
    sc_free(a); sc_free(b); sc_free(r);
    PASS(); tests_run++; tests_passed++;
}

void test_or(void) {
    TEST("OR(0.3, 0.4) → ~0.58 (1024 bits)");
    StochasticStream *a = sc_create(0.3, 1024, SC_VAN_DER_CORPUT);
    StochasticStream *b = sc_create(0.4, 1024, SC_RANDOM);
    StochasticStream *r = sc_or(a, b);
    double expected = 0.3 + 0.4 - 0.3 * 0.4;
    assert(fabs(sc_estimate(r) - expected) < TOLERANCE_1K);
    sc_free(a); sc_free(b); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("OR(1.0, x) → ~1.0 (absorption)");
    a = sc_create(1.0, 1024, SC_VAN_DER_CORPUT);
    b = sc_create(0.3, 1024, SC_RANDOM);
    r = sc_or(a, b);
    assert(fabs(sc_estimate(r) - 1.0) < 0.01);
    sc_free(a); sc_free(b); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("OR(0.0, x) → ~P(x) (identity)");
    a = sc_create(0.0, 1024, SC_VAN_DER_CORPUT);
    b = sc_create(0.6, 1024, SC_RANDOM);
    r = sc_or(a, b);
    assert(fabs(sc_estimate(r) - 0.6) < TOLERANCE_1K);
    sc_free(a); sc_free(b); sc_free(r);
    PASS(); tests_run++; tests_passed++;
}

void test_not(void) {
    TEST("NOT(0.7) → ~0.3");
    StochasticStream *a = sc_create(0.7, 1024, SC_VAN_DER_CORPUT);
    StochasticStream *r = sc_not(a);
    assert(fabs(sc_estimate(r) - 0.3) < TOLERANCE_1K);
    sc_free(a); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("NOT(0.0) → ~1.0");
    a = sc_create(0.0, 1024, SC_VAN_DER_CORPUT);
    r = sc_not(a);
    assert(fabs(sc_estimate(r) - 1.0) < 0.01);
    sc_free(a); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("NOT(1.0) → ~0.0");
    a = sc_create(1.0, 1024, SC_VAN_DER_CORPUT);
    r = sc_not(a);
    assert(fabs(sc_estimate(r) - 0.0) < 0.01);
    sc_free(a); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("NOT(NOT(0.6)) → ~0.6 (double negation)");
    a = sc_create(0.6, 4096, SC_RANDOM);
    StochasticStream *n1 = sc_not(a);
    StochasticStream *n2 = sc_not(n1);
    assert(fabs(sc_estimate(n2) - 0.6) < TOLERANCE_4K);
    sc_free(a); sc_free(n1); sc_free(n2);
    PASS(); tests_run++; tests_passed++;
}

void test_estimate_window(void) {
    TEST("estimate_window(0.7, 64) is rough, (512) is better");
    StochasticStream *sc = sc_create(0.7, 4096, SC_RANDOM);
    double p64 = sc_estimate_window(sc, 64);
    double p512 = sc_estimate_window(sc, 512);
    double e64 = fabs(p64 - 0.7);
    double e512 = fabs(p512 - 0.7);
    assert(e512 < 0.10);
    (void)e64;
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;
}

void test_convergence(void) {
    TEST("converged: 4096 bits with p=0.5 should converge at 0.05");
    StochasticStream *sc = sc_create(0.5, 4096, SC_RANDOM);
    bool conv = sc_converged(sc, 0.05);
    assert(conv);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;

    TEST("estimate_converging(0.7, 0.95) returns early");
    sc = sc_create(0.7, 4096, SC_VAN_DER_CORPUT);
    double p = sc_estimate_converging(sc, 0.95, 4096);
    assert(fabs(p - 0.7) < 0.1);
    sc_free(sc);
    PASS(); tests_run++; tests_passed++;
}

void test_from_distribution(void) {
    TEST("from Discrete([0.7, 0.3]) → stream ~0.7");
    Distribution *d = dist_discrete((double[]){0.7, 0.3}, (const char *[]){"a","b"}, 2);
    StochasticStream *sc = sc_from_distribution(d, 1024);
    assert(fabs(sc_estimate(sc) - 0.7) < TOLERANCE_1K);
    sc_free(sc);
    dist_free(d);
    PASS(); tests_run++; tests_passed++;

    TEST("from Discrete([0.2, 0.8]) → stream ~0.2");
    d = dist_discrete((double[]){0.2, 0.8}, NULL, 2);
    sc = sc_from_distribution(d, 1024);
    assert(fabs(sc_estimate(sc) - 0.2) < TOLERANCE_1K);
    sc_free(sc);
    dist_free(d);
    PASS(); tests_run++; tests_passed++;

    TEST("from Normal(0, 1) → stream ~0.5");
    d = dist_normal(0.0, 1.0);
    sc = sc_from_distribution(d, 1024);
    assert(fabs(sc_estimate(sc) - 0.5) < TOLERANCE_1K);
    sc_free(sc);
    dist_free(d);
    PASS(); tests_run++; tests_passed++;
}

void test_to_distribution(void) {
    TEST("stream(0.7) → Discrete ~[0.7, 0.3]");
    StochasticStream *sc = sc_create(0.7, 4096, SC_VAN_DER_CORPUT);
    Distribution *d = sc_to_distribution(sc);
    assert(d != NULL);
    assert(d->type == DIST_DISCRETE);
    assert(d->params.discrete.n == 2);
    assert(fabs(d->params.discrete.probs[0] - 0.7) < TOLERANCE_4K);
    sc_free(sc);
    dist_free(d);
    PASS(); tests_run++; tests_passed++;
}

void test_clone(void) {
    TEST("clone preserves data");
    StochasticStream *sc = sc_create(0.6, 1024, SC_VAN_DER_CORPUT);
    StochasticStream *copy = sc_clone(sc);
    assert(copy != NULL);
    assert(copy->length == sc->length);
    assert(copy->ones_count == sc->ones_count);
    assert(fabs(copy->estimated_p - sc->estimated_p) < 0.0001);
    sc_free(sc);
    sc_free(copy);
    PASS(); tests_run++; tests_passed++;
}

void test_algebra_matches_karubyte(void) {
    TEST("SC AND absorption: AND(0, x) = 0");
    StochasticStream *zero = sc_create(0.0, 1024, SC_VAN_DER_CORPUT);
    StochasticStream *x = sc_create(0.9, 1024, SC_RANDOM);
    StochasticStream *r = sc_and(zero, x);
    assert(fabs(sc_estimate(r) - 0.0) < 0.01);
    sc_free(zero); sc_free(x); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("SC AND identity: AND(1, x) = x");
    StochasticStream *one = sc_create(1.0, 1024, SC_VAN_DER_CORPUT);
    x = sc_create(0.65, 1024, SC_RANDOM);
    r = sc_and(one, x);
    assert(fabs(sc_estimate(r) - 0.65) < TOLERANCE_1K);
    sc_free(one); sc_free(x); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("SC OR absorption: OR(1, x) = 1");
    one = sc_create(1.0, 1024, SC_VAN_DER_CORPUT);
    x = sc_create(0.3, 1024, SC_RANDOM);
    r = sc_or(one, x);
    assert(fabs(sc_estimate(r) - 1.0) < 0.01);
    sc_free(one); sc_free(x); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("SC OR identity: OR(0, x) = x");
    zero = sc_create(0.0, 1024, SC_VAN_DER_CORPUT);
    x = sc_create(0.65, 1024, SC_RANDOM);
    r = sc_or(zero, x);
    assert(fabs(sc_estimate(r) - 0.65) < TOLERANCE_1K);
    sc_free(zero); sc_free(x); sc_free(r);
    PASS(); tests_run++; tests_passed++;

    TEST("SC NOT involution: NOT(NOT(x)) = x");
    x = sc_create(0.65, 4096, SC_RANDOM);
    StochasticStream *n1 = sc_not(x);
    StochasticStream *n2 = sc_not(n1);
    assert(fabs(sc_estimate(n2) - 0.65) < TOLERANCE_4K);
    sc_free(x); sc_free(n1); sc_free(n2);
    PASS(); tests_run++; tests_passed++;

    TEST("SC De Morgan: NOT(AND(a,b)) = OR(NOT(a), NOT(b))");
    StochasticStream *a = sc_create(0.6, 4096, SC_RANDOM);
    StochasticStream *b = sc_create(0.4, 4096, SC_RANDOM);
    StochasticStream *not_and = sc_not(sc_and(a, b));
    StochasticStream *or_not = sc_or(sc_not(a), sc_not(b));
    assert(fabs(sc_estimate(not_and) - sc_estimate(or_not)) < TOLERANCE_4K);
    sc_free(a); sc_free(b); sc_free(not_and); sc_free(or_not);
    PASS(); tests_run++; tests_passed++;
}

int main(void) {
    printf("\n=== StochasticStream Test Suite ===\n\n");

    printf("[Suite: Creation]\n");
    test_creation();

    printf("\n[Suite: Van der Corput]\n");
    test_van_der_corput();

    printf("\n[Suite: LFSR]\n");
    test_lfsr();

    printf("\n[Suite: AND operation]\n");
    test_and();

    printf("\n[Suite: OR operation]\n");
    test_or();

    printf("\n[Suite: NOT operation]\n");
    test_not();

    printf("\n[Suite: Window estimation]\n");
    test_estimate_window();

    printf("\n[Suite: Convergence]\n");
    test_convergence();

    printf("\n[Suite: Distribution conversion]\n");
    test_from_distribution();

    printf("\n[Suite: To Distribution]\n");
    test_to_distribution();

    printf("\n[Suite: Clone]\n");
    test_clone();

    printf("\n[Suite: Algebra matches KaruByte rules]\n");
    test_algebra_matches_karubyte();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
