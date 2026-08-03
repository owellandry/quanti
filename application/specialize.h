#ifndef QUANTI_SPECIALIZE_H
#define QUANTI_SPECIALIZE_H

#include "ir.h"

/*
 * Mark classical IR regions (no karu ops) for AOT specialization.
 * Sets IrInst.classical and IrModule.all_classical / uses_karu.
 */
void specialize_module(IrModule *m);

#endif /* QUANTI_SPECIALIZE_H */
