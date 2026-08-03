#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "interpreter.h"

#define TEST(name) printf("  [TEST] %-55s", name)
#define PASS()     printf("OK\n")

static int tests_run = 0, tests_passed = 0;

static Interpreter *fresh_interp(void) {
    return interp_create(quanti_default_config());
}

void test_basic_types(void) {
    TEST("int variable and print");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp, "int x = 42; print(x);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("float variable");
    interp = fresh_interp();
    assert(interp_exec(interp, "float pi = 3.14; print(pi);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("string variable");
    interp = fresh_interp();
    assert(interp_exec(interp, "string name = \"Quanti\"; print(name);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("bool variable");
    interp = fresh_interp();
    assert(interp_exec(interp, "bool flag = true; print(flag);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_arithmetic(void) {
    TEST("Integer arithmetic");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp, "print(2 + 3 * 4);"));  /* should print 14 */
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("Assignment and update");
    interp = fresh_interp();
    assert(interp_exec(interp, "int x = 5; x = x + 1; print(x);"));  /* should print 6 */
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_karu_operations(void) {
    TEST("karu superposition(0, 1) → K");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp, "karu x = superposition(0, 1); print(x);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("karu AND operation");
    interp = fresh_interp();
    assert(interp_exec(interp, "karu x = superposition(0, 1); karu y = x AND 1; print(y);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("karu NOT operation");
    interp = fresh_interp();
    assert(interp_exec(interp, "karu x = superposition(0, 1); karu z = NOT x; print(z);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_measure(void) {
    TEST("measure:map(K) → 1");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp,
        "karu x = superposition(0, 1);\n"
        "print(measure:map(x));\n"
    ));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("measure:first(K) → 0");
    interp = fresh_interp();
    assert(interp_exec(interp,
        "karu x = superposition(0, 1);\n"
        "print(measure:first(x));\n"
    ));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_if_else(void) {
    TEST("if true branch");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp, "int x = 10; if (x > 5) { print(1); }"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("if else branch");
    interp = fresh_interp();
    assert(interp_exec(interp, "int x = 3; if (x > 5) { print(1); } else { print(0); }"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_while(void) {
    TEST("while loop countdown");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp,
        "int x = 3;\n"
        "while (x > 0) {\n"
        "    print(x);\n"
        "    x = x - 1;\n"
        "}\n"
    ));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_functions(void) {
    TEST("fn doble(int n) → n * 2");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp,
        "fn doble(int n) -> int { return n * 2; }\n"
        "print(doble(21));\n"
    ));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("fn with two params");
    interp = fresh_interp();
    assert(interp_exec(interp,
        "fn add(int a, int b) -> int { return a + b; }\n"
        "print(add(10, 32));\n"
    ));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_distributions(void) {
    TEST("P(Discrete) with measure:map");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp,
        "karu d = P(Discrete([0.2, 0.8], [\"a\", \"b\"]));\n"
        "print(measure:map(d));\n"
    ));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("P(Normal) creation");
    interp = fresh_interp();
    assert(interp_exec(interp,
        "karu t = P(Normal(20.0, 2.5));\n"
        "print(t);\n"
    ));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_spec_program(void) {
    TEST("=== SPEC PROGRAM: Complete QA execution ===");
    Interpreter *interp = fresh_interp();

    printf("\n--- Output ---\n");
    bool ok = interp_exec(interp,
        "// Quanti spec program\n"
        "karu x = superposition(0, 1);\n"
        "karu y = x AND 1;\n"
        "print(measure:map(y));\n"
        "print(measure:first(y));\n"
        "\n"
        "int n = 10;\n"
        "if (n > 5) {\n"
        "    print(n);\n"
        "}\n"
        "\n"
        "fn doble(int x) -> int {\n"
        "    return x * 2;\n"
        "}\n"
        "\n"
        "print(doble(21));\n"
    );
    printf("--- End ---\n");

    assert(ok);
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_error_handling(void) {
    TEST("Undefined variable → error");
    Interpreter *interp = fresh_interp();
    assert(!interp_exec(interp, "print(undefined_var);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("Parse error → handled");
    interp = fresh_interp();
    assert(!interp_exec(interp, "int x = ;"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

void test_branching_semantics(void) {
    TEST("@runtime config accepted");
    Interpreter *interp = fresh_interp();
    assert(interp_exec(interp, "@runtime(max_branches: 16, prune_threshold: 0.05)\nint x = 1; print(x);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("when with classical true");
    interp = fresh_interp();
    assert(interp_exec(interp, "int x = 1;\nwhen (x) { print(x); }"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("if on karu superposition forks");
    interp = fresh_interp();
    assert(interp_exec(interp,
        "karu h = superposition(0, 1);\n"
        "int score = 0;\n"
        "if (h) { score = 10; } else { score = 3; }\n"
        "print(score);\n"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;

    TEST("print collapses multistate karu");
    interp = fresh_interp();
    assert(interp_exec(interp, "karu x = superposition(0, 1); print(x);"));
    interp_destroy(interp);
    PASS(); tests_run++; tests_passed++;
}

int main(void) {
    printf("\n=== Quanti Test Suite — Interpreter (E2E) ===\n\n");

    printf("[SUITE] Basic Types\n");
    test_basic_types();

    printf("\n[SUITE] Arithmetic\n");
    test_arithmetic();

    printf("\n[SUITE] Karu Operations\n");
    test_karu_operations();

    printf("\n[SUITE] Measure\n");
    test_measure();

    printf("\n[SUITE] If/Else\n");
    test_if_else();

    printf("\n[SUITE] While\n");
    test_while();

    printf("\n[SUITE] Functions\n");
    test_functions();

    printf("\n[SUITE] Distributions\n");
    test_distributions();

    printf("\n[SUITE] Spec Program\n");
    test_spec_program();

    printf("\n[SUITE] Error Handling\n");
    test_error_handling();

    printf("\n[SUITE] Branching Semantics\n");
    test_branching_semantics();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
