#ifndef QUANTI_LEXER_H
#define QUANTI_LEXER_H

#include <stddef.h>
#include <stdbool.h>

/*
 * Lexer del lenguaje QA.
 *
 * Convierte código fuente .qa en una secuencia de tokens.
 * Soporta: keywords, identificadores, literales (int, float, string),
 * operadores, puntuación, y tokens especiales de Quanti.
 */

typedef enum {
    /* ── Literals ──────────────────────── */
    TOK_INT_LIT,        /* 42, 0, -5            */
    TOK_FLOAT_LIT,      /* 3.14, 0.5            */
    TOK_STRING_LIT,     /* "hello"               */
    TOK_IDENT,          /* variable names        */

    /* ── Keywords ──────────────────────── */
    TOK_KARU,           /* karu                  */
    TOK_INT,            /* int                   */
    TOK_FLOAT,          /* float                 */
    TOK_STRING,         /* string                */
    TOK_BOOL,           /* bool                  */
    TOK_TRUE,           /* true                  */
    TOK_FALSE,          /* false                 */
    TOK_FN,             /* fn                    */
    TOK_RETURN,         /* return                */
    TOK_IF,             /* if                    */
    TOK_ELSE,           /* else                  */
    TOK_WHILE,          /* while                 */
    TOK_PRINT,          /* print                 */
    TOK_SUPERPOSITION,  /* superposition         */
    TOK_MEASURE,        /* measure               */
    TOK_NOT,            /* NOT                   */
    TOK_AND,            /* AND                   */
    TOK_OR,             /* OR                    */
    TOK_P_DIST,         /* P (distribution ctor) */
    TOK_NORMAL,         /* Normal                */
    TOK_DISCRETE,       /* Discrete              */
    TOK_UNIFORM,        /* Uniform               */
    TOK_PERSISTENT,     /* @persistent           */
    TOK_RUNTIME_CFG,    /* @runtime              */
    TOK_WHEN,           /* when                  */

    /* ── Operators ─────────────────────── */
    TOK_ASSIGN,         /* =                     */
    TOK_EQ,             /* ==                    */
    TOK_NEQ,            /* !=                    */
    TOK_LT,             /* <                     */
    TOK_GT,             /* >                     */
    TOK_LTE,            /* <=                    */
    TOK_GTE,            /* >=                    */
    TOK_PLUS,           /* +                     */
    TOK_MINUS,          /* -                     */
    TOK_STAR,           /* *                     */
    TOK_SLASH,          /* /                     */
    TOK_COLON,          /* :                     */
    TOK_ARROW,          /* ->                    */

    /* ── Punctuation ───────────────────── */
    TOK_LPAREN,         /* (                     */
    TOK_RPAREN,         /* )                     */
    TOK_LBRACE,         /* {                     */
    TOK_RBRACE,         /* }                     */
    TOK_LBRACKET,       /* [                     */
    TOK_RBRACKET,       /* ]                     */
    TOK_COMMA,          /* ,                     */
    TOK_SEMICOLON,      /* ;                     */
    TOK_DOT,            /* .                     */
    TOK_AT,             /* @                     */

    /* ── Special ───────────────────────── */
    TOK_EOF,            /* end of input          */
    TOK_ERROR           /* lexer error           */
} TokenType;

typedef struct {
    TokenType   type;
    const char *start;    /* puntero al inicio del token en el source */
    size_t      length;   /* longitud del lexema */
    int         line;     /* número de línea (1-based) */
    int         col;      /* columna (1-based) */
} Token;

typedef struct {
    const char *source;   /* código fuente completo */
    const char *current;  /* posición actual */
    int         line;
    int         col;
} Lexer;

/* ── API ────────────────────────────────────────────── */

void  lexer_init(Lexer *lexer, const char *source);
Token lexer_next(Lexer *lexer);
Token lexer_peek(Lexer *lexer);

/* ── Utilidades ─────────────────────────────────────── */

const char *token_type_name(TokenType type);
bool        token_equals(Token tok, const char *str);

/* Extrae el lexema como string (caller debe hacer free) */
char *token_to_string(Token tok);

#endif /* QUANTI_LEXER_H */
