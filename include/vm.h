#ifndef QUANTI_VM_H
#define QUANTI_VM_H

#include "ir.h"
#include "runtime.h"
#include <stdbool.h>

/*
 * Bytecode VM — executes Quenti IR against QuantiRuntime.
 */

bool vm_execute(IrModule *m, QuantiConfig config);

#endif /* QUANTI_VM_H */
