#ifndef QUANTI_PARSER_H
#define QUANTI_PARSER_H

#include "lexer.h"
#include "ast.h"

/*
 * Parser del lenguaje QA.
 * Recursive descent parser que produce un AST.
 */

typedef struct {
    Lexer    lexer;
    Token    current;
    Token    previous;
    bool     had_error;
    char     error_msg[256];
} Parser;

/* ── API ────────────────────────────────────────────── */

void     parser_init(Parser *p, const char *source);
Program  parser_parse(Parser *p);

#endif /* QUANTI_PARSER_H */
