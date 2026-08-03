#ifndef QUANTI_PORTS_H
#define QUANTI_PORTS_H

/*
 * Quenti — Hexagonal micromodule map (ports & adapters)
 *
 *   domain/           Core multi-state algebra (no I/O, no CLI)
 *     karubyte, distribution, stochastic, memory, collapse, branch, pruner
 *
 *   application/      Use-cases / inward ports
 *     runtime         Orchestrates domain for callers
 *     ir, typecheck, specialize
 *
 *   adapters/
 *     cli/            Driving adapter — process entry
 *     frontend/       Driving — QA lexer/parser/AST/interpreter
 *     backend/        Driven — VM + AOT codegen
 *
 * Dependency rule: adapters → application → domain
 * Domain never includes adapters.
 */

#include "runtime.h"
#include "ir.h"
#include "typecheck.h"
#include "specialize.h"

#endif /* QUANTI_PORTS_H */
