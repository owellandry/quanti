#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../include/lexer.h"
#include "../include/ast.h"
#include "../include/parser.h"

#define TEST(name) printf("  [TEST] %-55s", name)
#define PASS()     printf("OK\n")

static int tests_run = 0, tests_passed = 0;

static Program parse_source(const char *src, Parser *p) {
    parser_init(p, src);
    return parser_parse(p);
}

/* ── Tests ──────────────────────────────────────────── */

void test_var_decl(void) {
    Parser p;

    TEST("int x = 5;");
    Program prog = parse_source("int x = 5;", &p);
    assert(!p.had_error);
    assert(prog.count == 1);
    assert(prog.stmts[0]->type == NODE_VAR_DECL);
    assert(prog.stmts[0]->as.var_decl.var_type == QA_TYPE_INT);
    assert(strcmp(prog.stmts[0]->as.var_decl.var_name, "x") == 0);
    assert(prog.stmts[0]->as.var_decl.var_init->type == NODE_INT_LIT);
    assert(prog.stmts[0]->as.var_decl.var_init->as.int_lit.int_val == 5);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("karu k = superposition(0, 1);");
    prog = parse_source("karu k = superposition(0, 1);", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->type == NODE_VAR_DECL);
    assert(prog.stmts[0]->as.var_decl.var_type == QA_TYPE_KARU);
    assert(prog.stmts[0]->as.var_decl.var_init->type == NODE_SUPERPOSITION);
    assert(prog.stmts[0]->as.var_decl.var_init->as.superposition.super_count == 2);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("karu y = x AND 1;");
    prog = parse_source("karu y = x AND 1;", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.var_decl.var_init->type == NODE_KARU_AND);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("karu z = NOT x;");
    prog = parse_source("karu z = NOT x;", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.var_decl.var_init->type == NODE_KARU_NOT);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("string s = \"hello\";");
    prog = parse_source("string s = \"hello\";", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.var_decl.var_type == QA_TYPE_STRING);
    assert(prog.stmts[0]->as.var_decl.var_init->type == NODE_STRING_LIT);
    assert(strcmp(prog.stmts[0]->as.var_decl.var_init->as.string_lit.str_val, "hello") == 0);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_print(void) {
    Parser p;

    TEST("print(42);");
    Program prog = parse_source("print(42);", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->type == NODE_PRINT);
    assert(prog.stmts[0]->as.print.print_expr->type == NODE_INT_LIT);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("print(measure:map(x));");
    prog = parse_source("print(measure:map(x));", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->type == NODE_PRINT);
    assert(prog.stmts[0]->as.print.print_expr->type == NODE_MEASURE);
    assert(prog.stmts[0]->as.print.print_expr->as.measure.measure_mode == MEASURE_MAP);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("print(measure:sample(y));");
    prog = parse_source("print(measure:sample(y));", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.print.print_expr->as.measure.measure_mode == MEASURE_SAMPLE);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("print(measure:first(z));");
    prog = parse_source("print(measure:first(z));", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.print.print_expr->as.measure.measure_mode == MEASURE_FIRST);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_expressions(void) {
    Parser p;

    TEST("Arithmetic: 2 + 3 * 4 (precedence)");
    /* Wrapping in print to make it a statement */
    Program prog = parse_source("print(2 + 3 * 4);", &p);
    assert(!p.had_error);
    ASTNode *expr = prog.stmts[0]->as.print.print_expr;
    assert(expr->type == NODE_BINARY);
    assert(expr->as.binary.op == OP_ADD);
    assert(expr->as.binary.right->type == NODE_BINARY);
    assert(expr->as.binary.right->as.binary.op == OP_MUL);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("Comparison: x > 3");
    prog = parse_source("print(x > 3);", &p);
    assert(!p.had_error);
    expr = prog.stmts[0]->as.print.print_expr;
    assert(expr->type == NODE_BINARY);
    assert(expr->as.binary.op == OP_GT);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("Karu ops: a AND b OR c");
    prog = parse_source("print(a AND b OR c);", &p);
    assert(!p.had_error);
    expr = prog.stmts[0]->as.print.print_expr;
    /* AND binds tighter in left-to-right, then OR */
    assert(expr->type == NODE_KARU_OR);
    assert(expr->as.binary.left->type == NODE_KARU_AND);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_if_while(void) {
    Parser p;

    TEST("if (x > 0) { print(x); }");
    Program prog = parse_source("if (x > 0) { print(x); }", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->type == NODE_IF);
    assert(prog.stmts[0]->as.if_stmt.if_cond->type == NODE_BINARY);
    assert(prog.stmts[0]->as.if_stmt.if_then->type == NODE_BLOCK);
    assert(prog.stmts[0]->as.if_stmt.if_else == NULL);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("if/else");
    prog = parse_source("if (x == 1) { print(1); } else { print(0); }", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.if_stmt.if_else != NULL);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("while (x > 0) { x = x - 1; }");
    prog = parse_source("while (x > 0) { x = x - 1; }", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->type == NODE_WHILE);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_functions(void) {
    Parser p;

    TEST("fn doble(int n) -> int { return n * 2; }");
    Program prog = parse_source("fn doble(int n) -> int { return n * 2; }", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->type == NODE_FN_DECL);
    assert(strcmp(prog.stmts[0]->as.fn_decl.fn_name, "doble") == 0);
    assert(prog.stmts[0]->as.fn_decl.fn_param_count == 1);
    assert(prog.stmts[0]->as.fn_decl.fn_return_type == QA_TYPE_INT);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("fn with multiple params");
    prog = parse_source("fn add(int a, int b) -> int { return a + b; }", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.fn_decl.fn_param_count == 2);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_distributions(void) {
    Parser p;

    TEST("karu p = P(Discrete([0.7, 0.3], [\"a\", \"b\"]));");
    Program prog = parse_source("karu p = P(Discrete([0.7, 0.3], [\"a\", \"b\"]));", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.var_decl.var_init->type == NODE_P_DIST);
    assert(prog.stmts[0]->as.var_decl.var_init->as.p_dist.dist_type == DIST_AST_DISCRETE);
    assert(prog.stmts[0]->as.var_decl.var_init->as.p_dist.dist_arg_count == 2);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("karu t = P(Normal(20.0, 2.5));");
    prog = parse_source("karu t = P(Normal(20.0, 2.5));", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.var_decl.var_init->as.p_dist.dist_type == DIST_AST_NORMAL);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;

    TEST("karu u = P(Uniform(0.0, 100.0));");
    prog = parse_source("karu u = P(Uniform(0.0, 100.0));", &p);
    assert(!p.had_error);
    assert(prog.stmts[0]->as.var_decl.var_init->as.p_dist.dist_type == DIST_AST_UNIFORM);
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_full_program(void) {
    Parser p;

    TEST("Full spec program parses correctly");
    const char *src =
        "// Quanti test program\n"
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
        "print(doble(21));\n";

    Program prog = parse_source(src, &p);
    if (p.had_error) printf("ERROR: %s\n", p.error_msg);
    assert(!p.had_error);
    assert(prog.count == 8);  /* 3 decls + 3 prints + 1 if + 1 fn */
    program_free(&prog);
    PASS(); tests_run++; tests_passed++;
}

void test_error_recovery(void) {
    Parser p;

    TEST("Missing semicolon produces error");
    parse_source("int x = 5", &p);
    assert(p.had_error);
    PASS(); tests_run++; tests_passed++;

    TEST("Missing paren produces error");
    parse_source("print(42;", &p);
    assert(p.had_error);
    PASS(); tests_run++; tests_passed++;
}

/* ── Main ───────────────────────────────────────────── */

int main(void) {
    printf("\n=== Quanti Test Suite — Parser ===\n\n");

    printf("[SUITE] Variable Declarations\n");
    test_var_decl();

    printf("\n[SUITE] Print\n");
    test_print();

    printf("\n[SUITE] Expressions\n");
    test_expressions();

    printf("\n[SUITE] If/While\n");
    test_if_while();

    printf("\n[SUITE] Functions\n");
    test_functions();

    printf("\n[SUITE] Distributions\n");
    test_distributions();

    printf("\n[SUITE] Full Program\n");
    test_full_program();

    printf("\n[SUITE] Error Recovery\n");
    test_error_recovery();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
