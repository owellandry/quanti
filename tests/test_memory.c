#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "../include/karubyte.h"
#include "../include/distribution.h"
#include "../include/memory.h"

#define TEST(name) printf("  [TEST] %-50s", name)
#define PASS()     printf("OK\n")

static int tests_run    = 0;
static int tests_passed = 0;

/* ── Tests: Creación y registro ─────────────────────── */

void test_creation(void) {
    TEST("kmem_create produce memoria válida");
    KaruMemory *mem = kmem_create(16);
    assert(mem != NULL);
    assert(kmem_node_count(mem) == 0);
    kmem_free(mem);
    PASS(); tests_run++; tests_passed++;
}

void test_register(void) {
    KaruMemory *mem = kmem_create(16);

    TEST("Registrar un KaruByte");
    KaruByte x = karu_super();
    size_t idx = kmem_register(mem, x);
    assert(idx == 0);
    assert(kmem_node_count(mem) == 1);
    PASS(); tests_run++; tests_passed++;

    TEST("Registrar múltiples nodos");
    KaruByte y = karu_true();
    KaruByte z = karu_false();
    kmem_register(mem, y);
    kmem_register(mem, z);
    assert(kmem_node_count(mem) == 3);
    PASS(); tests_run++; tests_passed++;

    TEST("Buscar nodo por ID");
    KaruNode *found = kmem_find(mem, x.id);
    assert(found != NULL);
    assert(found->karu.state == KARU_SUPER);
    PASS(); tests_run++; tests_passed++;

    TEST("Buscar nodo inexistente → NULL");
    KaruNode *nope = kmem_find(mem, 99999);
    assert(nope == NULL);
    PASS(); tests_run++; tests_passed++;

    kmem_free(mem);
}

/* ── Tests: Dependencias ────────────────────────────── */

void test_dependencies(void) {
    KaruMemory *mem = kmem_create(16);

    /*  x(K) ← y depende de x
     *       ← z depende de x
     *  y    ← w depende de y
     */
    KaruByte x = karu_super();
    KaruByte y = karu_super();
    KaruByte z = karu_true();
    KaruByte w = karu_false();

    kmem_register(mem, x);
    kmem_register(mem, y);
    kmem_register(mem, z);
    kmem_register(mem, w);

    TEST("Agregar dependencia y → x");
    assert(kmem_add_dependency(mem, y.id, x.id));
    PASS(); tests_run++; tests_passed++;

    TEST("Agregar dependencia z → x");
    assert(kmem_add_dependency(mem, z.id, x.id));
    PASS(); tests_run++; tests_passed++;

    TEST("Agregar dependencia w → y");
    assert(kmem_add_dependency(mem, w.id, y.id));
    PASS(); tests_run++; tests_passed++;

    TEST("Edge count = 3");
    assert(kmem_edge_count(mem) == 3);
    PASS(); tests_run++; tests_passed++;

    TEST("Dependientes directos de x = {y, z}");
    size_t count = 0;
    uint32_t *deps = kmem_get_dependents(mem, x.id, &count);
    assert(count == 2);
    /* Verificar que y y z están en el resultado */
    int found_y = 0, found_z = 0;
    for (size_t i = 0; i < count; i++) {
        if (deps[i] == y.id) found_y = 1;
        if (deps[i] == z.id) found_z = 1;
    }
    assert(found_y && found_z);
    free(deps);
    PASS(); tests_run++; tests_passed++;

    TEST("Dependientes directos de y = {w}");
    deps = kmem_get_dependents(mem, y.id, &count);
    assert(count == 1);
    assert(deps[0] == w.id);
    free(deps);
    PASS(); tests_run++; tests_passed++;

    TEST("Dependientes directos de z = {} (hoja)");
    deps = kmem_get_dependents(mem, z.id, &count);
    assert(count == 0);
    free(deps);
    PASS(); tests_run++; tests_passed++;

    TEST("Evitar duplicados en dependencias");
    assert(kmem_add_dependency(mem, y.id, x.id));  /* duplicado */
    KaruNode *ny = kmem_find(mem, y.id);
    assert(ny->dep_count == 1);  /* sigue siendo 1, no 2 */
    PASS(); tests_run++; tests_passed++;

    TEST("Dependencia a nodo inexistente falla");
    assert(!kmem_add_dependency(mem, y.id, 99999));
    PASS(); tests_run++; tests_passed++;

    kmem_free(mem);
}

/* ── Tests: Cascada (transitive closure) ────────────── */

void test_cascade(void) {
    KaruMemory *mem = kmem_create(16);

    /*  x ← y ← w
     *    ← z
     */
    KaruByte x = karu_super();
    KaruByte y = karu_super();
    KaruByte z = karu_true();
    KaruByte w = karu_false();

    kmem_register(mem, x);
    kmem_register(mem, y);
    kmem_register(mem, z);
    kmem_register(mem, w);

    kmem_add_dependency(mem, y.id, x.id);
    kmem_add_dependency(mem, z.id, x.id);
    kmem_add_dependency(mem, w.id, y.id);

    TEST("Cascada de x = {y, z, w}");
    size_t count = 0;
    uint32_t *cascade = kmem_get_cascade(mem, x.id, &count);
    assert(count == 3);
    int found_y = 0, found_z = 0, found_w = 0;
    for (size_t i = 0; i < count; i++) {
        if (cascade[i] == y.id) found_y = 1;
        if (cascade[i] == z.id) found_z = 1;
        if (cascade[i] == w.id) found_w = 1;
    }
    assert(found_y && found_z && found_w);
    free(cascade);
    PASS(); tests_run++; tests_passed++;

    TEST("Cascada de y = {w}");
    cascade = kmem_get_cascade(mem, y.id, &count);
    assert(count == 1);
    assert(cascade[0] == w.id);
    free(cascade);
    PASS(); tests_run++; tests_passed++;

    TEST("Cascada de z = {} (hoja)");
    cascade = kmem_get_cascade(mem, z.id, &count);
    assert(count == 0);
    free(cascade);
    PASS(); tests_run++; tests_passed++;

    kmem_free(mem);
}

/* ── Tests: Colapso y resolución ────────────────────── */

void test_collapse_tracking(void) {
    KaruMemory *mem = kmem_create(16);

    KaruByte x = karu_super();
    KaruByte y = karu_super();

    kmem_register(mem, x);
    kmem_register(mem, y);
    kmem_add_dependency(mem, y.id, x.id);

    TEST("Deps no resueltas antes de colapso");
    assert(!kmem_deps_resolved(mem, y.id));
    PASS(); tests_run++; tests_passed++;

    TEST("Marcar x como colapsado");
    assert(kmem_mark_collapsed(mem, x.id));
    KaruNode *nx = kmem_find(mem, x.id);
    assert(nx->collapsed);
    PASS(); tests_run++; tests_passed++;

    TEST("Deps resueltas después de colapsar x");
    assert(kmem_deps_resolved(mem, y.id));
    PASS(); tests_run++; tests_passed++;

    TEST("Nodo determinista se registra como colapsado");
    KaruByte det = karu_true();
    kmem_register(mem, det);
    KaruNode *nd = kmem_find(mem, det.id);
    assert(nd->collapsed);  /* deterministas nacen colapsados */
    PASS(); tests_run++; tests_passed++;

    kmem_free(mem);
}

/* ── Tests: Crecimiento dinámico ────────────────────── */

void test_growth(void) {
    TEST("Registrar más nodos que capacidad inicial");
    KaruMemory *mem = kmem_create(4);  /* capacidad mínima (se ajusta a 16) */

    for (int i = 0; i < 50; i++) {
        KaruByte k = karu_super();
        kmem_register(mem, k);
    }
    assert(kmem_node_count(mem) == 50);
    kmem_free(mem);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Estadísticas ────────────────────────────── */

void test_stats(void) {
    KaruMemory *mem = kmem_create(16);

    KaruByte a = karu_super();
    KaruByte b = karu_true();
    KaruByte c = karu_false();

    kmem_register(mem, a);
    kmem_register(mem, b);
    kmem_register(mem, c);

    kmem_add_dependency(mem, b.id, a.id);
    kmem_add_dependency(mem, c.id, a.id);
    kmem_add_dependency(mem, c.id, b.id);

    TEST("node_count = 3");
    assert(kmem_node_count(mem) == 3);
    PASS(); tests_run++; tests_passed++;

    TEST("edge_count = 3");
    assert(kmem_edge_count(mem) == 3);
    PASS(); tests_run++; tests_passed++;

    kmem_free(mem);
}

/* ── Main ───────────────────────────────────────────── */

int main(void) {
    printf("\n=== Quanti Test Suite — KaruMemory (DAG) ===\n\n");

    printf("[SUITE] Creation\n");
    test_creation();

    printf("\n[SUITE] Register\n");
    test_register();

    printf("\n[SUITE] Dependencies\n");
    test_dependencies();

    printf("\n[SUITE] Cascade\n");
    test_cascade();

    printf("\n[SUITE] Collapse Tracking\n");
    test_collapse_tracking();

    printf("\n[SUITE] Dynamic Growth\n");
    test_growth();

    printf("\n[SUITE] Statistics\n");
    test_stats();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
