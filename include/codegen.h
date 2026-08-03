#ifndef QUANTI_CODEGEN_H
#define QUANTI_CODEGEN_H

#include "ir.h"
#include <stdbool.h>

/*
 * AOT codegen: IR → C source that links against libquanti.
 */

bool codegen_emit_c(const IrModule *m, const char *out_path);

/* Compile .qa → native binary via generated C + gcc.
 * Returns 0 on success. */
int codegen_build(const char *qa_path, const char *out_exe, bool use_lto);

#endif /* QUANTI_CODEGEN_H */
