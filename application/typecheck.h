#ifndef QUANTI_TYPECHECK_H
#define QUANTI_TYPECHECK_H

#include "ast.h"
#include <stdbool.h>

/*
 * Lightweight type check / inference over the AST.
 * Marks obvious type errors before IR lowering.
 */

typedef struct {
    bool had_error;
    char error_msg[512];
} TypeCheckResult;

TypeCheckResult typecheck_program(Program *prog);

#endif /* QUANTI_TYPECHECK_H */
