#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "../include/karubyte.h"
#include "../include/distribution.h"
#include "../include/memory.h"
#include "../include/collapse.h"

#define TEST(name) printf("  [TEST] %-50s", name)
#define PASS()     printf("OK\n")

static int tests_run    = 0;
static int tests_passed = 0;

/* ── Tests: Colapso individual ──────────────────────── */

void test_collapse_single(void) {
    TEST("MAP: 0 → 0 (ya determinista)");
    KaruByte r = collapse_single(karu_false(), COLLAPSE_MAP);
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("MAP: 1 → 1 (ya determinista)");
    r = collapse_single(karu_true(), COLLAPSE_MAP);
    assert(r.state == KARU_TRUE);
    PASS(); tests_run++; tests_passed++;

    TEST("MAP: K → 1 (desempate por orden)");
    r = collapse_single(karu_super(), COLLAPSE_MAP);
    assert(r.state == KARU_TRUE);
    PASS(); tests_run++; tests_passed++;

    TEST("FIRST: K → 0 (primera rama)");
    r = collapse_single(karu_super(), COLLAPSE_FIRST);
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("MAP: Ø → 0 (indefinido colapsa a falso)");
    r = collapse_single(karu_undef(), COLLAPSE_MAP);
    assert(r.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("MAP: P([0.7, 0.3]) → 0 (índice 0 más probable)");
    Distribution *d = dist_discrete((double[]){0.7, 0.3}, NULL, 2);
    KaruByte p = karu_prob(d);
    r = collapse_single(p, COLLAPSE_MAP);
    assert(r.state == KARU_FALSE);  /* MAP value = 0.0 → index 0 → FALSE */
    karu_free(&p);
    PASS(); tests_run++; tests_passed++;

    TEST("MAP: P([0.2, 0.8]) → 1 (índice 1 más probable)");
    d = dist_discrete((double[]){0.2, 0.8}, NULL, 2);
    p = karu_prob(d);
    r = collapse_single(p, COLLAPSE_MAP);
    assert(r.state == KARU_TRUE);  /* MAP value = 1.0 → index 1 → TRUE */
    karu_free(&p);
    PASS(); tests_run++; tests_passed++;

    TEST("FIRST: P → 0 (siempre primera rama)");
    d = dist_discrete((double[]){0.2, 0.8}, NULL, 2);
    p = karu_prob(d);
    r = collapse_single(p, COLLAPSE_FIRST);
    assert(r.state == KARU_FALSE);
    karu_free(&p);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Determinismo de MAP ─────────────────────── */

void test_map_determinism(void) {
    TEST("MAP es determinista: 100 colapsos de K → mismo resultado");
    KaruState first = collapse_single(karu_super(), COLLAPSE_MAP).state;
    for (int i = 0; i < 100; i++) {
        KaruByte r = collapse_single(karu_super(), COLLAPSE_MAP);
        assert(r.state == first);
    }
    PASS(); tests_run++; tests_passed++;

    TEST("MAP es determinista: 100 colapsos de P → mismo resultado");
    Distribution *d = dist_discrete((double[]){0.3, 0.7}, NULL, 2);
    KaruByte p = karu_prob(d);
    KaruState first_p = collapse_single(p, COLLAPSE_MAP).state;
    for (int i = 0; i < 100; i++) {
        KaruByte r = collapse_single(p, COLLAPSE_MAP);
        assert(r.state == first_p);
    }
    karu_free(&p);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Propagación ─────────────────────────────── */

void test_propagation(void) {
    /*  x(K) ← y(K) ← z(K)  */
    KaruMemory *mem = kmem_create(16);

    KaruByte x = karu_super();
    KaruByte y = karu_super();
    KaruByte z = karu_super();

    kmem_register(mem, x);
    kmem_register(mem, y);
    kmem_register(mem, z);

    kmem_add_dependency(mem, y.id, x.id);
    kmem_add_dependency(mem, z.id, y.id);

    TEST("Propagar colapso x → y → z (3 nodos)");
    size_t count = collapse_propagate(mem, x.id, COLLAPSE_MAP);
    assert(count == 3);
    PASS(); tests_run++; tests_passed++;

    TEST("Todos colapsados después de propagación");
    assert(kmem_find(mem, x.id)->collapsed);
    assert(kmem_find(mem, y.id)->collapsed);
    assert(kmem_find(mem, z.id)->collapsed);
    PASS(); tests_run++; tests_passed++;

    TEST("Todos son deterministas después de colapso");
    assert(karu_is_deterministic(kmem_find(mem, x.id)->karu));
    assert(karu_is_deterministic(kmem_find(mem, y.id)->karu));
    assert(karu_is_deterministic(kmem_find(mem, z.id)->karu));
    PASS(); tests_run++; tests_passed++;

    TEST("Re-colapsar nodo ya colapsado → 0");
    count = collapse_propagate(mem, x.id, COLLAPSE_MAP);
    assert(count == 0);
    PASS(); tests_run++; tests_passed++;

    kmem_free(mem);
}

/* ── Tests: @persistent ─────────────────────────────── */

void test_persistent(void) {
    /*  x(K) ← y(K, @persistent) ← z(K)  */
    KaruMemory *mem = kmem_create(16);

    KaruByte x = karu_super();
    KaruByte y = karu_super();
    y.persistent = true;
    KaruByte z = karu_super();

    kmem_register(mem, x);
    kmem_register(mem, y);
    kmem_register(mem, z);

    kmem_add_dependency(mem, y.id, x.id);
    kmem_add_dependency(mem, z.id, y.id);

    TEST("@persistent: y no colapsa en cascada");
    size_t count = collapse_propagate(mem, x.id, COLLAPSE_MAP);
    /* x colapsa, y no (@persistent), z no (dep y no resuelta) */
    assert(count == 1);
    assert(kmem_find(mem, x.id)->collapsed);
    assert(!kmem_find(mem, y.id)->collapsed);
    assert(!kmem_find(mem, z.id)->collapsed);
    PASS(); tests_run++; tests_passed++;

    kmem_free(mem);
}

/* ── Tests: collapse_all_non_persistent ──────────────── */

void test_collapse_all(void) {
    KaruMemory *mem = kmem_create(16);

    KaruByte a = karu_super();
    KaruByte b = karu_super();
    b.persistent = true;
    KaruByte c = karu_super();

    kmem_register(mem, a);
    kmem_register(mem, b);
    kmem_register(mem, c);

    TEST("collapse_all: colapsa 2, respeta 1 persistent");
    size_t count = collapse_all_non_persistent(mem, COLLAPSE_MAP);
    assert(count == 2);
    assert(kmem_find(mem, a.id)->collapsed);
    assert(!kmem_find(mem, b.id)->collapsed);
    assert(kmem_find(mem, c.id)->collapsed);
    PASS(); tests_run++; tests_passed++;

    kmem_free(mem);
}

/* ── Tests: mode_name ───────────────────────────────── */

void test_mode_names(void) {
    TEST("collapse_mode_name strings");
    assert(collapse_mode_name(COLLAPSE_MAP)[0] == 'm');
    assert(collapse_mode_name(COLLAPSE_SAMPLE)[0] == 'm');
    assert(collapse_mode_name(COLLAPSE_FIRST)[0] == 'm');
    PASS(); tests_run++; tests_passed++;
}

/* ── Main ───────────────────────────────────────────── */

int main(void) {
    printf("\n=== Quanti Test Suite — Collapse Engine ===\n\n");

    printf("[SUITE] Single Collapse\n");
    test_collapse_single();

    printf("\n[SUITE] MAP Determinism\n");
    test_map_determinism();

    printf("\n[SUITE] Propagation\n");
    test_propagation();

    printf("\n[SUITE] @persistent\n");
    test_persistent();

    printf("\n[SUITE] Collapse All\n");
    test_collapse_all();

    printf("\n[SUITE] Mode Names\n");
    test_mode_names();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
