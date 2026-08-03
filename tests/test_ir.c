#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "parser.h"
#include "typecheck.h"
#include "ir.h"
#include "vm.h"
#include "specialize.h"
#include "runtime.h"

#define TEST(name) printf("  [TEST] %-55s", name)
#define PASS()     printf("OK\n")

static int tests_run = 0, tests_passed = 0;

static Program parse_src(const char *src) {
    Parser p;
    parser_init(&p, src);
    Program prog = parser_parse(&p);
    assert(!p.had_error);
    return prog;
}

void test_typecheck_simple(void) {
    TEST("typecheck simple program");
    Program prog = parse_src("int x = 1; print(x);");
    TypeCheckResult tc = typecheck_program(&prog);
    assert(!tc.had_error);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_ir_compile_int_print(void) {
    TEST("ir_compile int print");
    Program prog = parse_src("int x = 1 + 2; print(x);");
    IrModule *m = ir_compile(&prog);
    assert(m && !m->had_error);
    assert(m->count > 0);
    assert(m->code[m->count - 1].op == IR_HALT);
    ir_free(m);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_vm_execute_int(void) {
    TEST("vm_execute int x = 1+2; print(x);");
    Program prog = parse_src("int x = 1 + 2; print(x);");
    IrModule *m = ir_compile(&prog);
    assert(m && !m->had_error);
    specialize_module(m);
    assert(vm_execute(m, quanti_default_config()));
    ir_free(m);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_ir_karu_super_measure(void) {
    TEST("ir_compile karu superposition + measure");
    Program prog = parse_src(
        "karu x = superposition(0, 1);\n"
        "int r = measure:map(x);\n"
        "print(r);\n");
    TypeCheckResult tc = typecheck_program(&prog);
    assert(!tc.had_error);
    IrModule *m = ir_compile(&prog);
    assert(m && !m->had_error);
    assert(m->uses_karu);
    specialize_module(m);
    assert(!m->all_classical);
    assert(vm_execute(m, quanti_default_config()));
    ir_free(m);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_specialize_classical(void) {
    TEST("specialize all_classical for pure int program");
    Program prog = parse_src("int x = 10; print(x);");
    IrModule *m = ir_compile(&prog);
    assert(m && !m->had_error);
    assert(!m->uses_karu);
    specialize_module(m);
    assert(m->all_classical);
    for (size_t i = 0; i < m->count; i++)
        assert(m->code[i].classical);
    ir_free(m);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

int main(void) {
    printf("\n=== IR / VM / typecheck tests ===\n\n");
    test_typecheck_simple();
    test_ir_compile_int_print();
    test_vm_execute_int();
    test_ir_karu_super_measure();
    test_specialize_classical();
    printf("\n%d / %d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
