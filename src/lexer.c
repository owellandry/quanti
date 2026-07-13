#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Helpers ────────────────────────────────────────── */

static bool is_at_end(Lexer *l) { return *l->current == '\0'; }

static char advance(Lexer *l) {
    char c = *l->current++;
    if (c == '\n') { l->line++; l->col = 1; }
    else           { l->col++; }
    return c;
}

static char peek(Lexer *l)      { return *l->current; }
static char peek_next(Lexer *l) { return is_at_end(l) ? '\0' : l->current[1]; }

static bool match(Lexer *l, char expected) {
    if (is_at_end(l) || *l->current != expected) return false;
    advance(l);
    return true;
}

static Token make_token(Lexer *l, TokenType type, const char *start, int line, int col) {
    return (Token){
        .type   = type,
        .start  = start,
        .length = (size_t)(l->current - start),
        .line   = line,
        .col    = col
    };
}

static Token error_token(Lexer *l, const char *msg) {
    return (Token){
        .type   = TOK_ERROR,
        .start  = msg,
        .length = strlen(msg),
        .line   = l->line,
        .col    = l->col
    };
}

/* ── Skip whitespace & comments ─────────────────────── */

static void skip_whitespace(Lexer *l) {
    for (;;) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(l);
        } else if (c == '/' && peek_next(l) == '/') {
            /* Line comment */
            while (!is_at_end(l) && peek(l) != '\n') advance(l);
        } else if (c == '/' && peek_next(l) == '*') {
            /* Block comment */
            advance(l); advance(l);
            while (!is_at_end(l) && !(peek(l) == '*' && peek_next(l) == '/'))
                advance(l);
            if (!is_at_end(l)) { advance(l); advance(l); } /* skip */ 
        } else {
            return;
        }
    }
}

/* ── Keyword table ──────────────────────────────────── */

typedef struct { const char *word; TokenType type; } Keyword;

static const Keyword keywords[] = {
    {"karu",          TOK_KARU},
    {"int",           TOK_INT},
    {"float",         TOK_FLOAT},
    {"string",        TOK_STRING},
    {"bool",          TOK_BOOL},
    {"true",          TOK_TRUE},
    {"false",         TOK_FALSE},
    {"fn",            TOK_FN},
    {"return",        TOK_RETURN},
    {"if",            TOK_IF},
    {"else",          TOK_ELSE},
    {"while",         TOK_WHILE},
    {"print",         TOK_PRINT},
    {"superposition", TOK_SUPERPOSITION},
    {"measure",       TOK_MEASURE},
    {"NOT",           TOK_NOT},
    {"AND",           TOK_AND},
    {"OR",            TOK_OR},
    {"P",             TOK_P_DIST},
    {"Normal",        TOK_NORMAL},
    {"Discrete",      TOK_DISCRETE},
    {"Uniform",       TOK_UNIFORM},
    {"when",          TOK_WHEN},
    {NULL, TOK_EOF}
};

static TokenType check_keyword(const char *start, size_t len) {
    for (int i = 0; keywords[i].word != NULL; i++) {
        if (strlen(keywords[i].word) == len &&
            memcmp(keywords[i].word, start, len) == 0)
            return keywords[i].type;
    }
    return TOK_IDENT;
}

/* ── Scan specific token types ──────────────────────── */

static Token scan_number(Lexer *l, const char *start, int line, int col) {
    while (isdigit(peek(l))) advance(l);

    if (peek(l) == '.' && isdigit(peek_next(l))) {
        advance(l); /* consume '.' */
        while (isdigit(peek(l))) advance(l);
        return make_token(l, TOK_FLOAT_LIT, start, line, col);
    }

    return make_token(l, TOK_INT_LIT, start, line, col);
}

static Token scan_string(Lexer *l, int line, int col) {
    const char *start = l->current - 1; /* include opening quote */

    while (!is_at_end(l) && peek(l) != '"') {
        if (peek(l) == '\\') advance(l); /* skip escape */
        advance(l);
    }

    if (is_at_end(l)) return error_token(l, "Unterminated string");
    advance(l); /* closing quote */

    return make_token(l, TOK_STRING_LIT, start, line, col);
}

static Token scan_identifier(Lexer *l, const char *start, int line, int col) {
    while (isalnum(peek(l)) || peek(l) == '_') advance(l);

    size_t len = (size_t)(l->current - start);
    TokenType type = check_keyword(start, len);
    return make_token(l, type, start, line, col);
}

static Token scan_at_keyword(Lexer *l, int line, int col) {
    const char *start = l->current - 1; /* include '@' */

    while (isalnum(peek(l)) || peek(l) == '_') advance(l);

    size_t len = (size_t)(l->current - start);

    if (len == 11 && memcmp(start, "@persistent", 11) == 0)
        return make_token(l, TOK_PERSISTENT, start, line, col);
    if (len == 8 && memcmp(start, "@runtime", 8) == 0)
        return make_token(l, TOK_RUNTIME_CFG, start, line, col);

    return make_token(l, TOK_AT, start, line, col - (int)(len - 1));
}

/* ── API ────────────────────────────────────────────── */

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source  = source;
    lexer->current = source;
    lexer->line    = 1;
    lexer->col     = 1;
}

Token lexer_next(Lexer *l) {
    skip_whitespace(l);

    if (is_at_end(l))
        return (Token){TOK_EOF, l->current, 0, l->line, l->col};

    const char *start = l->current;
    int line = l->line;
    int col  = l->col;
    char c   = advance(l);

    /* Numbers */
    if (isdigit(c)) return scan_number(l, start, line, col);

    /* Strings */
    if (c == '"') return scan_string(l, line, col);

    /* Identifiers / Keywords */
    if (isalpha(c) || c == '_') return scan_identifier(l, start, line, col);

    /* @ keywords */
    if (c == '@') return scan_at_keyword(l, line, col);

    /* Two-char operators */
    switch (c) {
    case '=': return make_token(l, match(l, '=') ? TOK_EQ  : TOK_ASSIGN, start, line, col);
    case '!': return match(l, '=') ? make_token(l, TOK_NEQ, start, line, col)
                                   : error_token(l, "Expected '=' after '!'");
    case '<': return make_token(l, match(l, '=') ? TOK_LTE : TOK_LT, start, line, col);
    case '>': return make_token(l, match(l, '=') ? TOK_GTE : TOK_GT, start, line, col);
    case '-': return make_token(l, match(l, '>') ? TOK_ARROW : TOK_MINUS, start, line, col);
    }

    /* Single-char tokens */
    switch (c) {
    case '+': return make_token(l, TOK_PLUS,      start, line, col);
    case '*': return make_token(l, TOK_STAR,      start, line, col);
    case '/': return make_token(l, TOK_SLASH,     start, line, col);
    case ':': return make_token(l, TOK_COLON,     start, line, col);
    case '(': return make_token(l, TOK_LPAREN,    start, line, col);
    case ')': return make_token(l, TOK_RPAREN,    start, line, col);
    case '{': return make_token(l, TOK_LBRACE,    start, line, col);
    case '}': return make_token(l, TOK_RBRACE,    start, line, col);
    case '[': return make_token(l, TOK_LBRACKET,  start, line, col);
    case ']': return make_token(l, TOK_RBRACKET,  start, line, col);
    case ',': return make_token(l, TOK_COMMA,     start, line, col);
    case ';': return make_token(l, TOK_SEMICOLON, start, line, col);
    case '.': return make_token(l, TOK_DOT,       start, line, col);
    }

    return error_token(l, "Unexpected character");
}

Token lexer_peek(Lexer *l) {
    /* Save state */
    const char *saved_current = l->current;
    int saved_line = l->line;
    int saved_col  = l->col;

    Token tok = lexer_next(l);

    /* Restore state */
    l->current = saved_current;
    l->line    = saved_line;
    l->col     = saved_col;

    return tok;
}

/* ── Utilidades ─────────────────────────────────────── */

const char *token_type_name(TokenType type) {
    switch (type) {
    case TOK_INT_LIT:       return "INT_LIT";
    case TOK_FLOAT_LIT:     return "FLOAT_LIT";
    case TOK_STRING_LIT:    return "STRING_LIT";
    case TOK_IDENT:         return "IDENT";
    case TOK_KARU:          return "KARU";
    case TOK_INT:           return "INT";
    case TOK_FLOAT:         return "FLOAT";
    case TOK_STRING:        return "STRING";
    case TOK_BOOL:          return "BOOL";
    case TOK_TRUE:          return "TRUE";
    case TOK_FALSE:         return "FALSE";
    case TOK_FN:            return "FN";
    case TOK_RETURN:        return "RETURN";
    case TOK_IF:            return "IF";
    case TOK_ELSE:          return "ELSE";
    case TOK_WHILE:         return "WHILE";
    case TOK_PRINT:         return "PRINT";
    case TOK_SUPERPOSITION: return "SUPERPOSITION";
    case TOK_MEASURE:       return "MEASURE";
    case TOK_NOT:           return "NOT";
    case TOK_AND:           return "AND";
    case TOK_OR:            return "OR";
    case TOK_P_DIST:        return "P_DIST";
    case TOK_NORMAL:        return "NORMAL";
    case TOK_DISCRETE:      return "DISCRETE";
    case TOK_UNIFORM:       return "UNIFORM";
    case TOK_PERSISTENT:    return "@PERSISTENT";
    case TOK_RUNTIME_CFG:   return "@RUNTIME";
    case TOK_WHEN:          return "WHEN";
    case TOK_ASSIGN:        return "=";
    case TOK_EQ:            return "==";
    case TOK_NEQ:           return "!=";
    case TOK_LT:            return "<";
    case TOK_GT:            return ">";
    case TOK_LTE:           return "<=";
    case TOK_GTE:           return ">=";
    case TOK_PLUS:          return "+";
    case TOK_MINUS:         return "-";
    case TOK_STAR:          return "*";
    case TOK_SLASH:         return "/";
    case TOK_COLON:         return ":";
    case TOK_ARROW:         return "->";
    case TOK_LPAREN:        return "(";
    case TOK_RPAREN:        return ")";
    case TOK_LBRACE:        return "{";
    case TOK_RBRACE:        return "}";
    case TOK_LBRACKET:      return "[";
    case TOK_RBRACKET:      return "]";
    case TOK_COMMA:         return ",";
    case TOK_SEMICOLON:     return ";";
    case TOK_DOT:           return ".";
    case TOK_AT:            return "@";
    case TOK_EOF:           return "EOF";
    case TOK_ERROR:         return "ERROR";
    }
    return "UNKNOWN";
}

bool token_equals(Token tok, const char *str) {
    size_t len = strlen(str);
    return tok.length == len && memcmp(tok.start, str, len) == 0;
}

char *token_to_string(Token tok) {
    char *s = malloc(tok.length + 1);
    if (!s) return NULL;
    memcpy(s, tok.start, tok.length);
    s[tok.length] = '\0';
    return s;
}
