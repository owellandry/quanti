#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "lexer.h"

#define TEST(name) printf("  [TEST] %-55s", name)
#define PASS()     printf("OK\n")

static int tests_run = 0, tests_passed = 0;

/* Helper: assert next token type */
static Token expect(Lexer *l, TokenType type) {
    Token t = lexer_next(l);
    if (t.type != type) {
        char *s = token_to_string(t);
        printf("FAIL: expected %s, got %s ('%s') at line %d\n",
               token_type_name(type), token_type_name(t.type), s ? s : "?", t.line);
        free(s);
        assert(0);
    }
    return t;
}

/* ── Tests: Keywords ────────────────────────────────── */

void test_keywords(void) {
    Lexer l;

    TEST("karu keyword");
    lexer_init(&l, "karu");
    expect(&l, TOK_KARU);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("All type keywords");
    lexer_init(&l, "int float string bool");
    expect(&l, TOK_INT); expect(&l, TOK_FLOAT);
    expect(&l, TOK_STRING); expect(&l, TOK_BOOL);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("Control flow keywords");
    lexer_init(&l, "if else while fn return when");
    expect(&l, TOK_IF); expect(&l, TOK_ELSE); expect(&l, TOK_WHILE);
    expect(&l, TOK_FN); expect(&l, TOK_RETURN); expect(&l, TOK_WHEN);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("Quanti-specific keywords");
    lexer_init(&l, "superposition measure print NOT AND OR");
    expect(&l, TOK_SUPERPOSITION); expect(&l, TOK_MEASURE);
    expect(&l, TOK_PRINT); expect(&l, TOK_NOT);
    expect(&l, TOK_AND); expect(&l, TOK_OR);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("Distribution keywords");
    lexer_init(&l, "P Normal Discrete Uniform");
    expect(&l, TOK_P_DIST); expect(&l, TOK_NORMAL);
    expect(&l, TOK_DISCRETE); expect(&l, TOK_UNIFORM);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("true/false literals");
    lexer_init(&l, "true false");
    expect(&l, TOK_TRUE); expect(&l, TOK_FALSE);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Literals ────────────────────────────────── */

void test_literals(void) {
    Lexer l;

    TEST("Integer literal");
    lexer_init(&l, "42");
    Token t = expect(&l, TOK_INT_LIT);
    assert(t.length == 2 && memcmp(t.start, "42", 2) == 0);
    PASS(); tests_run++; tests_passed++;

    TEST("Float literal");
    lexer_init(&l, "3.14");
    t = expect(&l, TOK_FLOAT_LIT);
    assert(t.length == 4);
    PASS(); tests_run++; tests_passed++;

    TEST("String literal");
    lexer_init(&l, "\"hello world\"");
    t = expect(&l, TOK_STRING_LIT);
    assert(t.length == 13);  /* including quotes */
    PASS(); tests_run++; tests_passed++;

    TEST("String with escape");
    lexer_init(&l, "\"hello\\nworld\"");
    t = expect(&l, TOK_STRING_LIT);
    assert(t.type == TOK_STRING_LIT);
    PASS(); tests_run++; tests_passed++;

    TEST("Identifier");
    lexer_init(&l, "my_variable");
    t = expect(&l, TOK_IDENT);
    assert(token_equals(t, "my_variable"));
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Operators ───────────────────────────────── */

void test_operators(void) {
    Lexer l;

    TEST("Assignment and equality");
    lexer_init(&l, "= == !=");
    expect(&l, TOK_ASSIGN); expect(&l, TOK_EQ); expect(&l, TOK_NEQ);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("Comparison operators");
    lexer_init(&l, "< > <= >=");
    expect(&l, TOK_LT); expect(&l, TOK_GT);
    expect(&l, TOK_LTE); expect(&l, TOK_GTE);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("Arithmetic operators");
    lexer_init(&l, "+ - * /");
    expect(&l, TOK_PLUS); expect(&l, TOK_MINUS);
    expect(&l, TOK_STAR); expect(&l, TOK_SLASH);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("Arrow and colon");
    lexer_init(&l, "-> :");
    expect(&l, TOK_ARROW); expect(&l, TOK_COLON);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Punctuation ─────────────────────────────── */

void test_punctuation(void) {
    Lexer l;

    TEST("All punctuation");
    lexer_init(&l, "( ) { } [ ] , ; .");
    expect(&l, TOK_LPAREN); expect(&l, TOK_RPAREN);
    expect(&l, TOK_LBRACE); expect(&l, TOK_RBRACE);
    expect(&l, TOK_LBRACKET); expect(&l, TOK_RBRACKET);
    expect(&l, TOK_COMMA); expect(&l, TOK_SEMICOLON);
    expect(&l, TOK_DOT);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: @ keywords ──────────────────────────────── */

void test_at_keywords(void) {
    Lexer l;

    TEST("@persistent");
    lexer_init(&l, "@persistent");
    expect(&l, TOK_PERSISTENT);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("@runtime");
    lexer_init(&l, "@runtime");
    expect(&l, TOK_RUNTIME_CFG);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Comments ────────────────────────────────── */

void test_comments(void) {
    Lexer l;

    TEST("Line comment skipped");
    lexer_init(&l, "karu // this is a comment\nint");
    expect(&l, TOK_KARU);
    expect(&l, TOK_INT);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("Block comment skipped");
    lexer_init(&l, "karu /* comment */ int");
    expect(&l, TOK_KARU);
    expect(&l, TOK_INT);
    expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Line tracking ───────────────────────────── */

void test_line_tracking(void) {
    Lexer l;

    TEST("Line numbers track correctly");
    lexer_init(&l, "karu\nint\nfloat");
    Token t1 = expect(&l, TOK_KARU);
    assert(t1.line == 1);
    Token t2 = expect(&l, TOK_INT);
    assert(t2.line == 2);
    Token t3 = expect(&l, TOK_FLOAT);
    assert(t3.line == 3);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: Full QA statements ──────────────────────── */

void test_full_statements(void) {
    Lexer l;

    TEST("karu x = superposition(0, 1);");
    lexer_init(&l, "karu x = superposition(0, 1);");
    expect(&l, TOK_KARU); expect(&l, TOK_IDENT); expect(&l, TOK_ASSIGN);
    expect(&l, TOK_SUPERPOSITION); expect(&l, TOK_LPAREN);
    expect(&l, TOK_INT_LIT); expect(&l, TOK_COMMA);
    expect(&l, TOK_INT_LIT); expect(&l, TOK_RPAREN);
    expect(&l, TOK_SEMICOLON); expect(&l, TOK_EOF);
    PASS(); tests_run++; tests_passed++;

    TEST("karu y = x AND 1;");
    lexer_init(&l, "karu y = x AND 1;");
    expect(&l, TOK_KARU); expect(&l, TOK_IDENT); expect(&l, TOK_ASSIGN);
    expect(&l, TOK_IDENT); expect(&l, TOK_AND);
    expect(&l, TOK_INT_LIT); expect(&l, TOK_SEMICOLON);
    PASS(); tests_run++; tests_passed++;

    TEST("print(measure:map(y));");
    lexer_init(&l, "print(measure:map(y));");
    expect(&l, TOK_PRINT); expect(&l, TOK_LPAREN);
    expect(&l, TOK_MEASURE); expect(&l, TOK_COLON);
    expect(&l, TOK_IDENT); expect(&l, TOK_LPAREN);
    expect(&l, TOK_IDENT); expect(&l, TOK_RPAREN);
    expect(&l, TOK_RPAREN); expect(&l, TOK_SEMICOLON);
    PASS(); tests_run++; tests_passed++;

    TEST("fn doble(int n) -> int { return n * 2; }");
    lexer_init(&l, "fn doble(int n) -> int { return n * 2; }");
    expect(&l, TOK_FN); expect(&l, TOK_IDENT);
    expect(&l, TOK_LPAREN); expect(&l, TOK_INT);
    expect(&l, TOK_IDENT); expect(&l, TOK_RPAREN);
    expect(&l, TOK_ARROW); expect(&l, TOK_INT);
    expect(&l, TOK_LBRACE); expect(&l, TOK_RETURN);
    expect(&l, TOK_IDENT); expect(&l, TOK_STAR);
    expect(&l, TOK_INT_LIT); expect(&l, TOK_SEMICOLON);
    expect(&l, TOK_RBRACE);
    PASS(); tests_run++; tests_passed++;

    TEST("karu p = P(Discrete([0.7, 0.3], [\"a\", \"b\"]));");
    lexer_init(&l, "karu p = P(Discrete([0.7, 0.3], [\"a\", \"b\"]));");
    expect(&l, TOK_KARU); expect(&l, TOK_IDENT); expect(&l, TOK_ASSIGN);
    expect(&l, TOK_P_DIST); expect(&l, TOK_LPAREN);
    expect(&l, TOK_DISCRETE); expect(&l, TOK_LPAREN);
    expect(&l, TOK_LBRACKET); expect(&l, TOK_FLOAT_LIT);
    expect(&l, TOK_COMMA); expect(&l, TOK_FLOAT_LIT);
    expect(&l, TOK_RBRACKET); expect(&l, TOK_COMMA);
    expect(&l, TOK_LBRACKET); expect(&l, TOK_STRING_LIT);
    expect(&l, TOK_COMMA); expect(&l, TOK_STRING_LIT);
    expect(&l, TOK_RBRACKET); expect(&l, TOK_RPAREN);
    expect(&l, TOK_RPAREN); expect(&l, TOK_SEMICOLON);
    PASS(); tests_run++; tests_passed++;
}

/* ── Tests: peek no consume ─────────────────────────── */

void test_peek(void) {
    Lexer l;

    TEST("lexer_peek no consume token");
    lexer_init(&l, "karu int");
    Token peeked = lexer_peek(&l);
    assert(peeked.type == TOK_KARU);
    Token actual = lexer_next(&l);
    assert(actual.type == TOK_KARU);
    actual = lexer_next(&l);
    assert(actual.type == TOK_INT);
    PASS(); tests_run++; tests_passed++;
}

/* ── Main ───────────────────────────────────────────── */

int main(void) {
    printf("\n=== Quanti Test Suite — Lexer ===\n\n");

    printf("[SUITE] Keywords\n");
    test_keywords();

    printf("\n[SUITE] Literals\n");
    test_literals();

    printf("\n[SUITE] Operators\n");
    test_operators();

    printf("\n[SUITE] Punctuation\n");
    test_punctuation();

    printf("\n[SUITE] @ Keywords\n");
    test_at_keywords();

    printf("\n[SUITE] Comments\n");
    test_comments();

    printf("\n[SUITE] Line Tracking\n");
    test_line_tracking();

    printf("\n[SUITE] Full Statements\n");
    test_full_statements();

    printf("\n[SUITE] Peek\n");
    test_peek();

    printf("\n=== Results: %d/%d passed ===\n\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
