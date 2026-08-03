#include "parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────── */

static void parser_advance(Parser *p) {
    p->previous = p->current;
    p->current = lexer_next(&p->lexer);
}

static bool check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static bool parser_match(Parser *p, TokenType type) {
    if (!check(p, type)) return false;
    parser_advance(p);
    return true;
}

static void parser_error(Parser *p, const char *msg) {
    if (p->had_error) return;
    p->had_error = true;
    snprintf(p->error_msg, sizeof(p->error_msg),
             "Line %d: %s (got '%s')",
             p->current.line, msg, token_type_name(p->current.type));
}

static bool parser_expect(Parser *p, TokenType type, const char *msg) {
    if (check(p, type)) { parser_advance(p); return true; }
    parser_error(p, msg);
    return false;
}

static char *prev_string(Parser *p) {
    return token_to_string(p->previous);
}

/* ── Forward declarations ───────────────────────────── */

static ASTNode *parse_expression(Parser *p);
static ASTNode *parse_statement(Parser *p);
static ASTNode *parse_block(Parser *p);

/* ── Expression parsing (precedence climbing) ───────── */

static ASTNode *parse_primary(Parser *p) {
    int line = p->current.line;

    /* Integer literal */
    if (parser_match(p, TOK_INT_LIT)) {
        ASTNode *node = ast_alloc(NODE_INT_LIT, line);
        char *s = token_to_string(p->previous);
        node->as.int_lit.int_val = atoi(s);
        free(s);
        return node;
    }

    /* Float literal */
    if (parser_match(p, TOK_FLOAT_LIT)) {
        ASTNode *node = ast_alloc(NODE_FLOAT_LIT, line);
        char *s = token_to_string(p->previous);
        node->as.float_lit.float_val = atof(s);
        free(s);
        return node;
    }

    /* String literal */
    if (parser_match(p, TOK_STRING_LIT)) {
        ASTNode *node = ast_alloc(NODE_STRING_LIT, line);
        /* Strip quotes */
        size_t len = p->previous.length - 2;
        node->as.string_lit.str_val = malloc(len + 1);
        memcpy(node->as.string_lit.str_val, p->previous.start + 1, len);
        node->as.string_lit.str_val[len] = '\0';
        return node;
    }

    /* Bool literals */
    if (parser_match(p, TOK_TRUE)) {
        ASTNode *node = ast_alloc(NODE_BOOL_LIT, line);
        node->as.bool_lit.bool_val = true;
        return node;
    }
    if (parser_match(p, TOK_FALSE)) {
        ASTNode *node = ast_alloc(NODE_BOOL_LIT, line);
        node->as.bool_lit.bool_val = false;
        return node;
    }

    /* superposition(args...) */
    if (parser_match(p, TOK_SUPERPOSITION)) {
        parser_expect(p, TOK_LPAREN, "Expected '(' after 'superposition'");
        size_t cap = 4;
        ASTNode **args = malloc(cap * sizeof(ASTNode *));
        size_t count = 0;

        if (!check(p, TOK_RPAREN)) {
            do {
                if (count >= cap) {
                    cap *= 2;
                    args = realloc(args, cap * sizeof(ASTNode *));
                }
                args[count++] = parse_expression(p);
            } while (parser_match(p, TOK_COMMA));
        }
        parser_expect(p, TOK_RPAREN, "Expected ')' after superposition args");

        ASTNode *node = ast_alloc(NODE_SUPERPOSITION, line);
        node->as.superposition.super_args = args;
        node->as.superposition.super_count = count;
        return node;
    }

    /* measure:mode(expr) */
    if (parser_match(p, TOK_MEASURE)) {
        parser_expect(p, TOK_COLON, "Expected ':' after 'measure'");

        MeasureMode mode = MEASURE_MAP;
        if (parser_match(p, TOK_IDENT)) {
            char *m = prev_string(p);
            if (strcmp(m, "map") == 0)         mode = MEASURE_MAP;
            else if (strcmp(m, "sample") == 0)  mode = MEASURE_SAMPLE;
            else if (strcmp(m, "first") == 0)   mode = MEASURE_FIRST;
            else parser_error(p, "Unknown measure mode");
            free(m);
        } else {
            parser_error(p, "Expected measure mode (map/sample/first)");
        }

        parser_expect(p, TOK_LPAREN, "Expected '(' after measure mode");
        ASTNode *expr = parse_expression(p);
        parser_expect(p, TOK_RPAREN, "Expected ')' after measure expression");

        ASTNode *node = ast_alloc(NODE_MEASURE, line);
        node->as.measure.measure_mode = mode;
        node->as.measure.measure_expr = expr;
        return node;
    }

    /* P(Dist(...)) */
    if (parser_match(p, TOK_P_DIST)) {
        parser_expect(p, TOK_LPAREN, "Expected '(' after 'P'");

        DistAstType dtype = DIST_AST_DISCRETE;
        if (parser_match(p, TOK_NORMAL))        dtype = DIST_AST_NORMAL;
        else if (parser_match(p, TOK_DISCRETE))  dtype = DIST_AST_DISCRETE;
        else if (parser_match(p, TOK_UNIFORM))   dtype = DIST_AST_UNIFORM;
        else parser_error(p, "Expected distribution type");

        parser_expect(p, TOK_LPAREN, "Expected '(' after distribution type");

        /* Parse args (can be arrays or scalars) */
        size_t cap = 4;
        ASTNode **args = malloc(cap * sizeof(ASTNode *));
        size_t count = 0;

        if (!check(p, TOK_RPAREN)) {
            do {
                if (count >= cap) { cap *= 2; args = realloc(args, cap * sizeof(ASTNode *)); }
                /* Array literal */
                if (parser_match(p, TOK_LBRACKET)) {
                    size_t acap = 4;
                    ASTNode **elems = malloc(acap * sizeof(ASTNode *));
                    size_t acount = 0;
                    if (!check(p, TOK_RBRACKET)) {
                        do {
                            if (acount >= acap) { acap *= 2; elems = realloc(elems, acap * sizeof(ASTNode *)); }
                            elems[acount++] = parse_expression(p);
                        } while (parser_match(p, TOK_COMMA));
                    }
                    parser_expect(p, TOK_RBRACKET, "Expected ']'");
                    ASTNode *arr = ast_alloc(NODE_ARRAY_LIT, line);
                    arr->as.array_lit.array_elems = elems;
                    arr->as.array_lit.array_count = acount;
                    args[count++] = arr;
                } else {
                    args[count++] = parse_expression(p);
                }
            } while (parser_match(p, TOK_COMMA));
        }
        parser_expect(p, TOK_RPAREN, "Expected ')' after distribution args");
        parser_expect(p, TOK_RPAREN, "Expected ')' after P(...)");

        ASTNode *node = ast_alloc(NODE_P_DIST, line);
        node->as.p_dist.dist_type = dtype;
        node->as.p_dist.dist_args = args;
        node->as.p_dist.dist_arg_count = count;
        return node;
    }

    /* NOT expr (unary karu) */
    if (parser_match(p, TOK_NOT)) {
        ASTNode *operand = parse_primary(p);
        ASTNode *node = ast_alloc(NODE_KARU_NOT, line);
        node->as.unary.operand = operand;
        return node;
    }

    /* Unary minus */
    if (parser_match(p, TOK_MINUS)) {
        ASTNode *operand = parse_primary(p);
        ASTNode *node = ast_alloc(NODE_UNARY, line);
        node->as.unary.operand = operand;
        return node;
    }

    /* Grouped expression */
    if (parser_match(p, TOK_LPAREN)) {
        ASTNode *expr = parse_expression(p);
        parser_expect(p, TOK_RPAREN, "Expected ')'");
        return expr;
    }

    /* Identifier or function call */
    if (parser_match(p, TOK_IDENT)) {
        char *name = prev_string(p);

        /* Function call: name(args) */
        if (parser_match(p, TOK_LPAREN)) {
            size_t cap = 4;
            ASTNode **args = malloc(cap * sizeof(ASTNode *));
            size_t count = 0;

            if (!check(p, TOK_RPAREN)) {
                do {
                    if (count >= cap) { cap *= 2; args = realloc(args, cap * sizeof(ASTNode *)); }
                    args[count++] = parse_expression(p);
                } while (parser_match(p, TOK_COMMA));
            }
            parser_expect(p, TOK_RPAREN, "Expected ')' after arguments");

            ASTNode *node = ast_alloc(NODE_CALL, line);
            node->as.call.call_name = name;
            node->as.call.call_args = args;
            node->as.call.call_arg_count = count;
            return node;
        }

        ASTNode *node = ast_alloc(NODE_IDENT, line);
        node->as.ident.ident_name = name;
        return node;
    }

    parser_error(p, "Expected expression");
    return ast_alloc(NODE_INT_LIT, line); /* error recovery */
}

static ASTNode *parse_multiplicative(Parser *p) {
    ASTNode *left = parse_primary(p);

    while (check(p, TOK_STAR) || check(p, TOK_SLASH)) {
        int line = p->current.line;
        BinaryOp op = check(p, TOK_STAR) ? OP_MUL : OP_DIV;
        parser_advance(p);

        ASTNode *right = parse_primary(p);
        ASTNode *node = ast_alloc(NODE_BINARY, line);
        node->as.binary.op = op;
        node->as.binary.left = left;
        node->as.binary.right = right;
        left = node;
    }
    return left;
}

static ASTNode *parse_additive(Parser *p) {
    ASTNode *left = parse_multiplicative(p);

    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        int line = p->current.line;
        BinaryOp op = check(p, TOK_PLUS) ? OP_ADD : OP_SUB;
        parser_advance(p);

        ASTNode *right = parse_multiplicative(p);
        ASTNode *node = ast_alloc(NODE_BINARY, line);
        node->as.binary.op = op;
        node->as.binary.left = left;
        node->as.binary.right = right;
        left = node;
    }
    return left;
}

static ASTNode *parse_comparison(Parser *p) {
    ASTNode *left = parse_additive(p);

    while (check(p, TOK_EQ) || check(p, TOK_NEQ) ||
           check(p, TOK_LT) || check(p, TOK_GT) ||
           check(p, TOK_LTE) || check(p, TOK_GTE)) {
        int line = p->current.line;
        BinaryOp op;
        switch (p->current.type) {
        case TOK_EQ:  op = OP_EQ;  break;
        case TOK_NEQ: op = OP_NEQ; break;
        case TOK_LT:  op = OP_LT;  break;
        case TOK_GT:  op = OP_GT;  break;
        case TOK_LTE: op = OP_LTE; break;
        case TOK_GTE: op = OP_GTE; break;
        default:      op = OP_EQ;  break;
        }
        parser_advance(p);

        ASTNode *right = parse_additive(p);
        ASTNode *node = ast_alloc(NODE_BINARY, line);
        node->as.binary.op = op;
        node->as.binary.left = left;
        node->as.binary.right = right;
        left = node;
    }
    return left;
}

static ASTNode *parse_karu_ops(Parser *p) {
    ASTNode *left = parse_comparison(p);

    while (check(p, TOK_AND) || check(p, TOK_OR)) {
        int line = p->current.line;
        NodeType ntype = check(p, TOK_AND) ? NODE_KARU_AND : NODE_KARU_OR;
        parser_advance(p);

        ASTNode *right = parse_comparison(p);
        ASTNode *node = ast_alloc(ntype, line);
        node->as.binary.left = left;
        node->as.binary.right = right;
        left = node;
    }
    return left;
}

static ASTNode *parse_expression(Parser *p) {
    return parse_karu_ops(p);
}

/* ── Statement parsing ──────────────────────────────── */

static QAType parse_type(Parser *p) {
    if (parser_match(p, TOK_INT))    return QA_TYPE_INT;
    if (parser_match(p, TOK_FLOAT))  return QA_TYPE_FLOAT;
    if (parser_match(p, TOK_STRING)) return QA_TYPE_STRING;
    if (parser_match(p, TOK_BOOL))   return QA_TYPE_BOOL;
    if (parser_match(p, TOK_KARU))   return QA_TYPE_KARU;
    parser_error(p, "Expected type name");
    return QA_TYPE_INT;
}

static ASTNode *parse_var_decl(Parser *p, QAType type) {
    int line = p->previous.line;

    parser_expect(p, TOK_IDENT, "Expected variable name");
    char *name = prev_string(p);

    ASTNode *init = NULL;
    if (parser_match(p, TOK_ASSIGN)) {
        init = parse_expression(p);
    }

    /* Check for @persistent */
    bool persistent = false;
    if (parser_match(p, TOK_PERSISTENT)) {
        persistent = true;
    }

    parser_expect(p, TOK_SEMICOLON, "Expected ';' after variable declaration");

    ASTNode *node = ast_alloc(NODE_VAR_DECL, line);
    node->as.var_decl.var_type = type;
    node->as.var_decl.var_name = name;
    node->as.var_decl.var_init = init;
    node->as.var_decl.var_persistent = persistent;
    return node;
}

static ASTNode *parse_print(Parser *p) {
    int line = p->previous.line;
    parser_expect(p, TOK_LPAREN, "Expected '(' after 'print'");
    ASTNode *expr = parse_expression(p);
    parser_expect(p, TOK_RPAREN, "Expected ')' after print expression");
    parser_expect(p, TOK_SEMICOLON, "Expected ';' after print");

    ASTNode *node = ast_alloc(NODE_PRINT, line);
    node->as.print.print_expr = expr;
    return node;
}

static ASTNode *parse_if(Parser *p) {
    int line = p->previous.line;
    parser_expect(p, TOK_LPAREN, "Expected '(' after 'if'");
    ASTNode *cond = parse_expression(p);
    parser_expect(p, TOK_RPAREN, "Expected ')' after condition");

    ASTNode *then_branch = parse_block(p);
    ASTNode *else_branch = NULL;
    if (parser_match(p, TOK_ELSE)) {
        else_branch = parse_block(p);
    }

    ASTNode *node = ast_alloc(NODE_IF, line);
    node->as.if_stmt.if_cond = cond;
    node->as.if_stmt.if_then = then_branch;
    node->as.if_stmt.if_else = else_branch;
    return node;
}

static ASTNode *parse_when(Parser *p) {
    int line = p->previous.line;
    parser_expect(p, TOK_LPAREN, "Expected '(' after 'when'");
    ASTNode *cond = parse_expression(p);
    parser_expect(p, TOK_RPAREN, "Expected ')' after when condition");
    ASTNode *body = parse_block(p);

    ASTNode *node = ast_alloc(NODE_WHEN, line);
    node->as.when_stmt.when_cond = cond;
    node->as.when_stmt.when_body = body;
    return node;
}

static ASTNode *parse_runtime_cfg(Parser *p) {
    int line = p->previous.line;
    parser_expect(p, TOK_LPAREN, "Expected '(' after '@runtime'");

    ASTNode *node = ast_alloc(NODE_RUNTIME_CFG, line);
    node->as.runtime_cfg.max_branches = 0;
    node->as.runtime_cfg.prune_threshold = -1.0;
    node->as.runtime_cfg.has_max_branches = false;
    node->as.runtime_cfg.has_prune_threshold = false;

    if (!check(p, TOK_RPAREN)) {
        do {
            parser_expect(p, TOK_IDENT, "Expected config key");
            char *key = prev_string(p);
            parser_expect(p, TOK_COLON, "Expected ':' after config key");

            if (strcmp(key, "max_branches") == 0) {
                parser_expect(p, TOK_INT_LIT, "Expected integer for max_branches");
                char *s = prev_string(p);
                node->as.runtime_cfg.max_branches = (size_t)atoi(s);
                node->as.runtime_cfg.has_max_branches = true;
                free(s);
            } else if (strcmp(key, "prune_threshold") == 0) {
                if (parser_match(p, TOK_FLOAT_LIT) || parser_match(p, TOK_INT_LIT)) {
                    char *s = prev_string(p);
                    node->as.runtime_cfg.prune_threshold = atof(s);
                    node->as.runtime_cfg.has_prune_threshold = true;
                    free(s);
                } else {
                    parser_error(p, "Expected number for prune_threshold");
                }
            } else {
                parser_error(p, "Unknown @runtime key");
            }
            free(key);
        } while (parser_match(p, TOK_COMMA));
    }

    parser_expect(p, TOK_RPAREN, "Expected ')' after @runtime config");
    /* Optional semicolon */
    parser_match(p, TOK_SEMICOLON);
    return node;
}

static ASTNode *parse_while(Parser *p) {
    int line = p->previous.line;
    parser_expect(p, TOK_LPAREN, "Expected '(' after 'while'");
    ASTNode *cond = parse_expression(p);
    parser_expect(p, TOK_RPAREN, "Expected ')' after condition");
    ASTNode *body = parse_block(p);

    ASTNode *node = ast_alloc(NODE_WHILE, line);
    node->as.while_stmt.while_cond = cond;
    node->as.while_stmt.while_body = body;
    return node;
}

static ASTNode *parse_fn_decl(Parser *p) {
    int line = p->previous.line;
    parser_expect(p, TOK_IDENT, "Expected function name");
    char *name = prev_string(p);

    parser_expect(p, TOK_LPAREN, "Expected '(' after function name");

    /* Parse params */
    size_t cap = 4;
    Param *params = malloc(cap * sizeof(Param));
    size_t count = 0;

    if (!check(p, TOK_RPAREN)) {
        do {
            if (count >= cap) { cap *= 2; params = realloc(params, cap * sizeof(Param)); }
            QAType ptype = parse_type(p);
            parser_expect(p, TOK_IDENT, "Expected parameter name");
            params[count].type = ptype;
            params[count].name = prev_string(p);
            count++;
        } while (parser_match(p, TOK_COMMA));
    }
    parser_expect(p, TOK_RPAREN, "Expected ')' after parameters");

    /* Return type */
    QAType ret_type = QA_TYPE_VOID;
    if (parser_match(p, TOK_ARROW)) {
        ret_type = parse_type(p);
    }

    ASTNode *body = parse_block(p);

    ASTNode *node = ast_alloc(NODE_FN_DECL, line);
    node->as.fn_decl.fn_name = name;
    node->as.fn_decl.fn_params = params;
    node->as.fn_decl.fn_param_count = count;
    node->as.fn_decl.fn_return_type = ret_type;
    node->as.fn_decl.fn_body = body;
    return node;
}

static ASTNode *parse_return(Parser *p) {
    int line = p->previous.line;
    ASTNode *expr = NULL;
    if (!check(p, TOK_SEMICOLON))
        expr = parse_expression(p);
    parser_expect(p, TOK_SEMICOLON, "Expected ';' after return");

    ASTNode *node = ast_alloc(NODE_RETURN, line);
    node->as.ret.ret_expr = expr;
    return node;
}

static ASTNode *parse_block(Parser *p) {
    int line = p->current.line;
    parser_expect(p, TOK_LBRACE, "Expected '{'");

    size_t cap = 8;
    ASTNode **stmts = malloc(cap * sizeof(ASTNode *));
    size_t count = 0;

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (count >= cap) { cap *= 2; stmts = realloc(stmts, cap * sizeof(ASTNode *)); }
        stmts[count++] = parse_statement(p);
        if (p->had_error) break;
    }

    parser_expect(p, TOK_RBRACE, "Expected '}'");

    ASTNode *node = ast_alloc(NODE_BLOCK, line);
    node->as.block.block_stmts = stmts;
    node->as.block.block_count = count;
    return node;
}

static ASTNode *parse_statement(Parser *p) {
    /* Variable declarations */
    if (check(p, TOK_INT) || check(p, TOK_FLOAT) ||
        check(p, TOK_STRING) || check(p, TOK_BOOL) || check(p, TOK_KARU)) {
        QAType type = parse_type(p);
        return parse_var_decl(p, type);
    }

    if (parser_match(p, TOK_PRINT))  return parse_print(p);
    if (parser_match(p, TOK_IF))     return parse_if(p);
    if (parser_match(p, TOK_WHEN))   return parse_when(p);
    if (parser_match(p, TOK_RUNTIME_CFG)) return parse_runtime_cfg(p);
    if (parser_match(p, TOK_WHILE))  return parse_while(p);
    if (parser_match(p, TOK_FN))     return parse_fn_decl(p);
    if (parser_match(p, TOK_RETURN)) return parse_return(p);

    /* Assignment: ident = expr; */
    if (check(p, TOK_IDENT)) {
        /* peek ahead to see if it's assignment */
        Lexer saved = p->lexer;
        Token saved_current = p->current;
        parser_advance(p);  /* consume ident */

        if (check(p, TOK_ASSIGN)) {
            char *name = token_to_string(saved_current);
            parser_advance(p);  /* consume '=' */
            ASTNode *value = parse_expression(p);
            parser_expect(p, TOK_SEMICOLON, "Expected ';' after assignment");

            ASTNode *node = ast_alloc(NODE_ASSIGN, saved_current.line);
            node->as.assign.assign_name = name;
            node->as.assign.assign_value = value;
            return node;
        }

        /* Not assignment — restore and parse as expression statement */
        p->lexer = saved;
        p->current = saved_current;
    }

    /* Expression statement */
    int line = p->current.line;
    ASTNode *expr = parse_expression(p);
    parser_expect(p, TOK_SEMICOLON, "Expected ';' after expression");

    ASTNode *node = ast_alloc(NODE_EXPR_STMT, line);
    node->as.expr_stmt.expr = expr;
    return node;
}

/* ── API ────────────────────────────────────────────── */

void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->had_error = false;
    p->error_msg[0] = '\0';
    p->current = lexer_next(&p->lexer);
}

Program parser_parse(Parser *p) {
    Program prog = {NULL, 0};
    size_t cap = 16;
    prog.stmts = malloc(cap * sizeof(ASTNode *));

    while (!check(p, TOK_EOF) && !p->had_error) {
        if (prog.count >= cap) {
            cap *= 2;
            prog.stmts = realloc(prog.stmts, cap * sizeof(ASTNode *));
        }
        prog.stmts[prog.count++] = parse_statement(p);
    }

    return prog;
}
