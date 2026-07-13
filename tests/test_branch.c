#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "../include/karubyte.h"
#include "../include/distribution.h"
#include "../include/memory.h"
#include "../include/branch.h"

#define TEST(name) printf("  [TEST] %-50s", name)
#define PASS()     printf("OK\n")

static int tests_run    = 0;
static int tests_passed = 0;

/* ── Tests: Creación ────────────────────────────────── */

void test_creation(void) {
    TEST("branch_create válido");
    BranchManager *mgr = branch_create(64, 0.01);
    assert(mgr != NULL);
    assert(branch_active_count(mgr) == 0);
    branch_free(mgr);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Fork de K ───────────────────────────────── */

void test_fork_super(void) {
    BranchManager *mgr = branch_create(64, 0.01);

    TEST("Fork de K → 2 ramas");
    KaruByte x = karu_super();
    size_t created = branch_fork(mgr, x);
    assert(created == 2);
    assert(branch_active_count(mgr) == 2);
    PASS(); tests_run++; tests_passed++;

    TEST("Rama 0 tiene valor FALSE");
    Branch *b0 = &mgr->branches[0];
    KaruByte *v0 = branch_get_local(mgr, b0->id, x.id);
    assert(v0 != NULL);
    assert(v0->state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("Rama 1 tiene valor TRUE");
    Branch *b1 = &mgr->branches[1];
    KaruByte *v1 = branch_get_local(mgr, b1->id, x.id);
    assert(v1 != NULL);
    assert(v1->state == KARU_TRUE);
    PASS(); tests_run++; tests_passed++;

    TEST("Ambas ramas peso 0.5");
    assert(b0->weight == 0.5);
    assert(b1->weight == 0.5);
    PASS(); tests_run++; tests_passed++;

    branch_free(mgr);
}

/* ── Tests: Fork de P(Discrete) ─────────────────────── */

void test_fork_prob(void) {
    BranchManager *mgr = branch_create(64, 0.01);

    TEST("Fork de P([0.7, 0.3]) → 2 ramas");
    Distribution *d = dist_discrete((double[]){0.7, 0.3}, NULL, 2);
    KaruByte p = karu_prob(d);
    size_t created = branch_fork(mgr, p);
    assert(created == 2);
    PASS(); tests_run++; tests_passed++;

    TEST("Pesos correctos derivados de distribución");
    assert(mgr->branches[0].weight == 0.7);
    assert(mgr->branches[1].weight == 0.3);
    PASS(); tests_run++; tests_passed++;

    karu_free(&p);
    branch_free(mgr);
}

/* ── Tests: Fork respeta max_branches ───────────────── */

void test_max_branches(void) {
    BranchManager *mgr = branch_create(4, 0.01);

    TEST("Fork respeta max_branches");
    KaruByte x = karu_super();
    branch_fork(mgr, x);  /* 2 ramas */
    assert(branch_active_count(mgr) == 2);

    KaruByte y = karu_super();
    branch_fork(mgr, y);  /* 2 más = 4 total */
    assert(branch_active_count(mgr) == 4);

    KaruByte z = karu_super();
    size_t created = branch_fork(mgr, z);  /* límite alcanzado */
    assert(created == 0);
    assert(branch_active_count(mgr) == 4);
    PASS(); tests_run++; tests_passed++;

    branch_free(mgr);
}

/* ── Tests: Estado local ────────────────────────────── */

void test_locals(void) {
    BranchManager *mgr = branch_create(64, 0.01);

    KaruByte x = karu_super();
    branch_fork(mgr, x);

    Branch *b0 = &mgr->branches[0];

    TEST("Escribir y leer variable local");
    KaruByte val = karu_true();
    branch_set_local(mgr, b0->id, 999, val);
    KaruByte *got = branch_get_local(mgr, b0->id, 999);
    assert(got != NULL);
    assert(got->state == KARU_TRUE);
    PASS(); tests_run++; tests_passed++;

    TEST("Sobrescribir variable local");
    KaruByte val2 = karu_false();
    branch_set_local(mgr, b0->id, 999, val2);
    got = branch_get_local(mgr, b0->id, 999);
    assert(got->state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("Variable no existe en otra rama");
    Branch *b1 = &mgr->branches[1];
    got = branch_get_local(mgr, b1->id, 999);
    assert(got == NULL);
    PASS(); tests_run++; tests_passed++;

    branch_free(mgr);
}

/* ── Tests: Merge ───────────────────────────────────── */

void test_merge(void) {
    BranchManager *mgr = branch_create(64, 0.01);

    KaruByte x = karu_super();
    branch_fork(mgr, x);  /* rama 0: FALSE, rama 1: TRUE */

    TEST("Merge con valores divergentes → K (superposición)");
    KaruByte merged = branch_merge(mgr, x.id);
    assert(merged.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("Merge con valores iguales → determinista");
    Branch *b1 = &mgr->branches[1];
    branch_set_local(mgr, b1->id, x.id, karu_false());  /* ahora ambas FALSE */
    merged = branch_merge(mgr, x.id);
    assert(merged.state == KARU_FALSE);
    PASS(); tests_run++; tests_passed++;

    TEST("Merge de variable inexistente → Ø");
    merged = branch_merge(mgr, 99999);
    assert(merged.state == KARU_UNDEF);
    PASS(); tests_run++; tests_passed++;

    branch_free(mgr);
}

/* ── Tests: Poda ────────────────────────────────────── */

void test_prune(void) {
    BranchManager *mgr = branch_create(64, 0.1);

    TEST("Poda elimina ramas con peso < umbral");
    Distribution *d = dist_discrete((double[]){0.8, 0.15, 0.05}, NULL, 3);
    KaruByte p = karu_prob(d);
    branch_fork(mgr, p);

    /* 3 ramas: peso 0.8, 0.15, 0.05.
     * Pero la de 0.05 fue filtrada en fork (< prune_threshold 0.1).
     * Así que solo hay 2 ramas. */
    assert(branch_active_count(mgr) == 2);

    /* Forzar poda ahora no debería cambiar nada (ambas > 0.1) */
    size_t pruned = branch_prune(mgr);
    assert(pruned == 0);
    assert(branch_active_count(mgr) == 2);
    PASS(); tests_run++; tests_passed++;

    TEST("Poda con umbral dinámico");
    mgr->prune_threshold = 0.5;  /* subir umbral */
    pruned = branch_prune(mgr);
    assert(pruned == 1);  /* 0.15 < 0.5 → podada */
    assert(branch_active_count(mgr) == 1);
    PASS(); tests_run++; tests_passed++;

    karu_free(&p);
    branch_free(mgr);
}

/* ── Tests: Fork determinista no genera ramas ───────── */

void test_no_fork_deterministic(void) {
    BranchManager *mgr = branch_create(64, 0.01);

    TEST("Fork de FALSE → 0 ramas");
    assert(branch_fork(mgr, karu_false()) == 0);
    PASS(); tests_run++; tests_passed++;

    TEST("Fork de TRUE → 0 ramas");
    assert(branch_fork(mgr, karu_true()) == 0);
    PASS(); tests_run++; tests_passed++;

    TEST("Fork de Ø → 0 ramas");
    assert(branch_fork(mgr, karu_undef()) == 0);
    PASS(); tests_run++; tests_passed++;

    branch_free(mgr);
}

/* ── Main ───────────────────────────────────────────── */

int main(void) {
    printf("\n=== Quanti Test Suite — Branch Manager ===\n\n");

    printf("[SUITE] Creation\n");
    test_creation();

    printf("\n[SUITE] Fork K\n");
    test_fork_super();

    printf("\n[SUITE] Fork P\n");
    test_fork_prob();

    printf("\n[SUITE] Max Branches\n");
    test_max_branches();

    printf("\n[SUITE] Locals\n");
    test_locals();

    printf("\n[SUITE] Merge\n");
    test_merge();

    printf("\n[SUITE] Prune\n");
    test_prune();

    printf("\n[SUITE] No Fork Deterministic\n");
    test_no_fork_deterministic();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
