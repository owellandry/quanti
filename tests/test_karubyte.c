#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "../include/karubyte.h"
#include "../include/distribution.h"

#define TEST(name) printf("  [TEST] %-45s", name)
#define PASS()     printf("OK\n")

static int tests_run    = 0;
static int tests_passed = 0;

/* ── Tests: Creación de KaruByte ────────────────────── */

void test_creation(void) {
    TEST("karu_false → state == KARU_FALSE");
    KaruByte f = karu_false();
    assert(f.state == KARU_FALSE);
    assert(karu_is_deterministic(f));
    PASS(); tests_run++; tests_passed++;

    TEST("karu_true → state == KARU_TRUE");
    KaruByte t = karu_true();
    assert(t.state == KARU_TRUE);
    assert(karu_is_deterministic(t));
    PASS(); tests_run++; tests_passed++;

    TEST("karu_super → state == KARU_SUPER");
    KaruByte k = karu_super();
    assert(k.state == KARU_SUPER);
    assert(karu_is_superposition(k));
    PASS(); tests_run++; tests_passed++;

    TEST("karu_undef → state == KARU_UNDEF");
    KaruByte u = karu_undef();
    assert(u.state == KARU_UNDEF);
    assert(karu_is_undefined(u));
    PASS(); tests_run++; tests_passed++;

    TEST("karu_prob → state == KARU_PROB");
    Distribution *d = dist_discrete((double[]){0.7, 0.3}, (const char *[]){"a","b"}, 2);
    KaruByte p = karu_prob(d);
    assert(p.state == KARU_PROB);
    assert(karu_is_probabilistic(p));
    karu_free(&p);
    PASS(); tests_run++; tests_passed++;

    TEST("IDs son únicos");
    KaruByte a = karu_false();
    KaruByte b = karu_true();
    assert(a.id != b.id);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Álgebra AND ─────────────────────────────── */

void test_and(void) {
    KaruByte f = karu_false();
    KaruByte t = karu_true();
    KaruByte k = karu_super();
    KaruByte u = karu_undef();

    TEST("0 AND 0 = 0");
    KaruByte r = karu_and(f, f);
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("0 AND 1 = 0");
    r = karu_and(f, t);
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("1 AND 1 = 1");
    r = karu_and(t, t);
    assert(r.state == KARU_TRUE);
    karu_free(&r);
    PASS(); tests_run++; tests_passed++;

    TEST("1 AND K = K");
    r = karu_and(t, k);
    assert(r.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("K AND K = K");
    r = karu_and(k, k);
    assert(r.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("0 AND K = 0 (absorción)");
    r = karu_and(f, k);
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("Ø AND 1 = Ø");
    r = karu_and(u, t);
    assert(r.state == KARU_UNDEF);
    PASS(); tests_run++; tests_passed++;

    TEST("0 AND Ø = 0 (0 absorbe incluso a Ø)");
    r = karu_and(f, u);
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("K AND Ø = Ø");
    r = karu_and(k, u);
    assert(r.state == KARU_UNDEF);
    PASS(); tests_run++; tests_passed++;

    TEST("1 AND P = P");
    Distribution *d = dist_discrete((double[]){0.6, 0.4}, NULL, 2);
    KaruByte p = karu_prob(d);
    r = karu_and(t, p);
    assert(r.state == KARU_PROB);
    karu_free(&r);
    karu_free(&p);
    PASS(); tests_run++; tests_passed++;

    TEST("P AND P = P (intersección)");
    Distribution *d1 = dist_discrete((double[]){0.5, 0.5}, NULL, 2);
    Distribution *d2 = dist_discrete((double[]){0.8, 0.2}, NULL, 2);
    KaruByte p1 = karu_prob(d1);
    KaruByte p2 = karu_prob(d2);
    r = karu_and(p1, p2);
    assert(r.state == KARU_PROB);
    karu_free(&r);
    karu_free(&p1);
    karu_free(&p2);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Álgebra OR ──────────────────────────────── */

void test_or(void) {
    KaruByte f = karu_false();
    KaruByte t = karu_true();
    KaruByte k = karu_super();
    KaruByte u = karu_undef();

    TEST("0 OR 0 = 0");
    KaruByte r = karu_or(f, f);
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("0 OR 1 = 1");
    r = karu_or(f, t);
    assert(r.state == KARU_TRUE);
    karu_free(&r);
    PASS(); tests_run++; tests_passed++;

    TEST("1 OR K = 1 (absorción)");
    r = karu_or(t, k);
    assert(r.state == KARU_TRUE);
    PASS(); tests_run++; tests_passed++;

    TEST("0 OR K = K");
    r = karu_or(f, k);
    assert(r.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("K OR K = K");
    r = karu_or(k, k);
    assert(r.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("Ø OR 1 = 1");
    r = karu_or(u, t);
    assert(r.state == KARU_TRUE);
    PASS(); tests_run++; tests_passed++;

    TEST("Ø OR Ø = Ø");
    r = karu_or(u, u);
    assert(r.state == KARU_UNDEF);
    PASS(); tests_run++; tests_passed++;

    TEST("K OR Ø = K (superposición domina)");
    r = karu_or(k, u);
    assert(r.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("0 OR Ø = Ø");
    r = karu_or(f, u);
    assert(r.state == KARU_UNDEF);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Álgebra NOT ─────────────────────────────── */

void test_not(void) {
    TEST("NOT 0 = 1");
    KaruByte r = karu_not(karu_false());
    assert(r.state == KARU_TRUE);
    PASS(); tests_run++; tests_passed++;

    TEST("NOT 1 = 0");
    r = karu_not(karu_true());
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("NOT K = K");
    r = karu_not(karu_super());
    assert(r.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("NOT Ø = Ø");
    r = karu_not(karu_undef());
    assert(r.state == KARU_UNDEF);
    PASS(); tests_run++; tests_passed++;

    TEST("NOT P = P (distribución complementada)");
    Distribution *d = dist_discrete((double[]){0.7, 0.3}, NULL, 2);
    KaruByte p = karu_prob(d);
    r = karu_not(p);
    assert(r.state == KARU_PROB);
    assert(r.dist != NULL);
    karu_free(&r);
    karu_free(&p);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Distribuciones ──────────────────────────── */

void test_distributions(void) {
    TEST("dist_discrete válida");
    Distribution *d = dist_discrete((double[]){0.5, 0.3, 0.2}, (const char *[]){"a","b","c"}, 3);
    assert(d != NULL);
    assert(dist_is_valid(d));
    dist_free(d);
    PASS(); tests_run++; tests_passed++;

    TEST("dist_normal válida");
    Distribution *n = dist_normal(0.0, 1.0);
    assert(n != NULL);
    assert(dist_is_valid(n));
    dist_free(n);
    PASS(); tests_run++; tests_passed++;

    TEST("dist_uniform válida");
    Distribution *u = dist_uniform(0.0, 10.0);
    assert(u != NULL);
    assert(dist_is_valid(u));
    dist_free(u);
    PASS(); tests_run++; tests_passed++;

    TEST("dist_map_value discrete → índice mayor prob");
    d = dist_discrete((double[]){0.1, 0.7, 0.2}, NULL, 3);
    assert((int)dist_map_value(d) == 1);
    dist_free(d);
    PASS(); tests_run++; tests_passed++;

    TEST("dist_map_value normal → media");
    n = dist_normal(42.0, 5.0);
    assert(fabs(dist_map_value(n) - 42.0) < 1e-9);
    dist_free(n);
    PASS(); tests_run++; tests_passed++;

    TEST("dist_clone produce copia independiente");
    d = dist_discrete((double[]){0.5, 0.5}, NULL, 2);
    Distribution *c = dist_clone(d);
    assert(c != NULL);
    assert(c != d);
    assert(c->params.discrete.probs != d->params.discrete.probs);
    dist_free(d);
    dist_free(c);
    PASS(); tests_run++; tests_passed++;

    TEST("dist_complement invierte probabilidades");
    d = dist_discrete((double[]){0.9, 0.1}, NULL, 2);
    Distribution *comp = dist_complement(d);
    assert(comp != NULL);
    /* 1-0.9=0.1, 1-0.1=0.9 → normalizado: 0.1, 0.9 */
    assert(comp->params.discrete.probs[0] < comp->params.discrete.probs[1]);
    dist_free(d);
    dist_free(comp);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Conmutatividad ──────────────────────────── */

void test_commutativity(void) {
    KaruByte states[] = {karu_false(), karu_true(), karu_super(), karu_undef()};
    const char *names[] = {"0", "1", "K", "Ø"};

    for (int i = 0; i < 4; i++) {
        for (int j = i+1; j < 4; j++) {
            char buf[64];

            snprintf(buf, sizeof(buf), "AND conmutativo: %s AND %s", names[i], names[j]);
            TEST(buf);
            KaruByte ab = karu_and(states[i], states[j]);
            KaruByte ba = karu_and(states[j], states[i]);
            assert(ab.state == ba.state);
            karu_free(&ab); karu_free(&ba);
            PASS(); tests_run++; tests_passed++;

            snprintf(buf, sizeof(buf), "OR conmutativo: %s OR %s", names[i], names[j]);
            TEST(buf);
            ab = karu_or(states[i], states[j]);
            ba = karu_or(states[j], states[i]);
            assert(ab.state == ba.state);
            karu_free(&ab); karu_free(&ba);
            PASS(); tests_run++; tests_passed++;
        }
    }
}

/* ── Main ───────────────────────────────────────────── */

int main(void) {
    printf("\n=== Quanti Test Suite — KaruByte & Distribution ===\n\n");

    printf("[SUITE] Creation\n");
    test_creation();

    printf("\n[SUITE] AND Algebra\n");
    test_and();

    printf("\n[SUITE] OR Algebra\n");
    test_or();

    printf("\n[SUITE] NOT Algebra\n");
    test_not();

    printf("\n[SUITE] Distributions\n");
    test_distributions();

    printf("\n[SUITE] Commutativity\n");
    test_commutativity();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
