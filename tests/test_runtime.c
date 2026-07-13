#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include "../include/runtime.h"

#define TEST(name) printf("  [TEST] %-55s", name)
#define PASS()     printf("OK\n")

static int tests_run    = 0;
static int tests_passed = 0;

/* ── Tests: Lifecycle ───────────────────────────────── */

void test_lifecycle(void) {
    TEST("quanti_init y quanti_destroy");
    QuantiRuntime *rt = quanti_init(quanti_default_config());
    assert(rt != NULL);
    assert(quanti_node_count(rt) == 0);
    quanti_destroy(rt);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Creación registra en DAG ────────────────── */

void test_creation(void) {
    QuantiRuntime *rt = quanti_init(quanti_default_config());

    TEST("quanti_false registra en DAG");
    KaruByte f = quanti_false(rt);
    assert(f.state == KARU_FALSE);
    assert(quanti_node_count(rt) == 1);
    PASS(); tests_run++; tests_passed++;

    TEST("quanti_superposition registra en DAG");
    KaruByte k = quanti_superposition(rt);
    assert(k.state == KARU_SUPER);
    assert(quanti_node_count(rt) == 2);
    PASS(); tests_run++; tests_passed++;

    TEST("quanti_prob registra en DAG");
    Distribution *d = dist_discrete((double[]){0.6, 0.4}, NULL, 2);
    KaruByte p = quanti_prob(rt, d);
    assert(p.state == KARU_PROB);
    assert(quanti_node_count(rt) == 3);
    PASS(); tests_run++; tests_passed++;

    quanti_destroy(rt);
}

/* ── Tests: Operaciones con dependencias ────────────── */

void test_operations(void) {
    QuantiRuntime *rt = quanti_init(quanti_default_config());

    KaruByte x = quanti_superposition(rt);
    KaruByte y = quanti_true(rt);

    TEST("quanti_and registra resultado + dependencias");
    KaruByte r = quanti_and(rt, x, y);
    assert(r.state == KARU_SUPER);  /* K AND 1 = K */
    assert(quanti_node_count(rt) == 3);  /* x, y, r */
    PASS(); tests_run++; tests_passed++;

    TEST("quanti_or funciona");
    KaruByte r2 = quanti_or(rt, x, y);
    assert(r2.state == KARU_TRUE);  /* K OR 1 = 1 */
    PASS(); tests_run++; tests_passed++;

    TEST("quanti_not funciona");
    KaruByte r3 = quanti_not(rt, x);
    assert(r3.state == KARU_SUPER);  /* NOT K = K */
    PASS(); tests_run++; tests_passed++;

    quanti_destroy(rt);
}

/* ── Tests: Colapso end-to-end ──────────────────────── */

void test_collapse_e2e(void) {
    QuantiRuntime *rt = quanti_init(quanti_default_config());

    TEST("measure:map de K → determinista");
    KaruByte x = quanti_superposition(rt);
    KaruByte collapsed = quanti_measure_default(rt, x);
    assert(karu_is_deterministic(collapsed));
    karu_free(&collapsed);
    PASS(); tests_run++; tests_passed++;

    TEST("measure:map propaga a dependientes");
    QuantiRuntime *rt2 = quanti_init(quanti_default_config());
    KaruByte a = quanti_superposition(rt2);
    KaruByte b = quanti_and(rt2, a, quanti_true(rt2));  /* b = K AND 1 = K, depende de a */
    quanti_measure(rt2, a, COLLAPSE_MAP);
    /* b debería estar colapsado también */
    KaruNode *nb = kmem_find(rt2->memory, b.id);
    assert(nb != NULL);
    assert(nb->collapsed);
    quanti_destroy(rt2);
    PASS(); tests_run++; tests_passed++;

    TEST("measure:first siempre da FALSE para K");
    QuantiRuntime *rt3 = quanti_init(quanti_default_config());
    KaruByte k = quanti_superposition(rt3);
    KaruByte first = quanti_measure(rt3, k, COLLAPSE_FIRST);
    assert(first.state == KARU_FALSE);
    karu_free(&first);
    quanti_destroy(rt3);
    PASS(); tests_run++; tests_passed++;

    quanti_destroy(rt);
}

/* ── Tests: Branching end-to-end ────────────────────── */

void test_branching_e2e(void) {
    QuantiRuntime *rt = quanti_init(quanti_default_config());

    TEST("fork + merge cycle");
    KaruByte x = quanti_superposition(rt);
    size_t branches = quanti_fork(rt, x);
    assert(branches == 2);
    assert(quanti_active_branches(rt) == 2);

    /* Las ramas tienen valores divergentes → merge da K */
    KaruByte merged = quanti_merge(rt, x.id);
    assert(merged.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("prune elimina ramas de bajo peso");
    /* Crear ramas con distribución desigual */
    QuantiRuntime *rt2 = quanti_init(quanti_default_config());
    Distribution *d = dist_discrete((double[]){0.98, 0.02}, NULL, 2);
    KaruByte p = quanti_prob(rt2, d);
    quanti_fork(rt2, p);
    assert(quanti_active_branches(rt2) == 2);

    /* Subir umbral de poda */
    rt2->pruner.weight_threshold = 0.05;
    size_t pruned = quanti_prune(rt2);
    assert(pruned == 1);  /* 0.02 < 0.05 → podada */
    assert(quanti_active_branches(rt2) == 1);
    quanti_destroy(rt2);
    PASS(); tests_run++; tests_passed++;

    quanti_destroy(rt);
}

/* ── Test: Escenario completo de la spec ────────────── */

void test_spec_scenario(void) {
    TEST("Spec scenario: karu x = superposition(0,1); y = x AND 1");
    QuantiRuntime *rt = quanti_init(quanti_default_config());

    /* karu x = superposition(0, 1); */
    KaruByte x = quanti_superposition(rt);
    assert(x.state == KARU_SUPER);

    /* karu y = x AND 1; */
    KaruByte one = quanti_true(rt);
    KaruByte y = quanti_and(rt, x, one);
    assert(y.state == KARU_SUPER);  /* K AND 1 = K */

    /* print(measure:map(y)); → 1 */
    KaruByte map_result = quanti_measure(rt, y, COLLAPSE_MAP);
    assert(map_result.state == KARU_TRUE);
    karu_free(&map_result);

    quanti_destroy(rt);

    /* Segundo runtime para measure:first */
    rt = quanti_init(quanti_default_config());
    x = quanti_superposition(rt);
    one = quanti_true(rt);
    y = quanti_and(rt, x, one);

    /* print(measure:first(y)); → 0 */
    KaruByte first_result = quanti_measure(rt, y, COLLAPSE_FIRST);
    assert(first_result.state == KARU_FALSE);
    karu_free(&first_result);

    quanti_destroy(rt);
    PASS(); tests_run++; tests_passed++;
}

/* ── Test: Escenario IA agéntica ────────────────────── */

void test_agentic_scenario(void) {
    TEST("Agentic AI: múltiples hipótesis con colapso tardío");
    QuantiRuntime *rt = quanti_init(quanti_default_config());

    /*
     * karu intencion = superposition(Comprar, Explorar, Comparar)
     * Simplificado a: P([0.5, 0.3, 0.2])
     */
    Distribution *d = dist_discrete(
        (double[]){0.5, 0.3, 0.2},
        (const char *[]){"comprar", "explorar", "comparar"},
        3
    );
    KaruByte intencion = quanti_prob(rt, d);
    assert(intencion.state == KARU_PROB);

    /* El runtime mantiene las 3 posibilidades sin colapsar */
    assert(!kmem_find(rt->memory, intencion.id)->collapsed);

    /* Fork genera ramas ponderadas */
    size_t branches = quanti_fork(rt, intencion);
    assert(branches == 3);
    assert(quanti_active_branches(rt) == 3);

    /* measure:map colapsa al más probable (index 0 = "comprar", peso 0.5) */
    KaruByte resultado = quanti_measure(rt, intencion, COLLAPSE_MAP);
    assert(karu_is_deterministic(resultado));
    karu_free(&resultado);

    quanti_destroy(rt);
    PASS(); tests_run++; tests_passed++;
}

/* ── Main ───────────────────────────────────────────── */

int main(void) {
    printf("\n=== Quanti Test Suite — Full Runtime ===\n\n");

    printf("[SUITE] Lifecycle\n");
    test_lifecycle();

    printf("\n[SUITE] Creation\n");
    test_creation();

    printf("\n[SUITE] Operations\n");
    test_operations();

    printf("\n[SUITE] Collapse E2E\n");
    test_collapse_e2e();

    printf("\n[SUITE] Branching E2E\n");
    test_branching_e2e();

    printf("\n[SUITE] Spec Scenario\n");
    test_spec_scenario();

    printf("\n[SUITE] Agentic AI Scenario\n");
    test_agentic_scenario();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
