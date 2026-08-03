/*
 * benchmark_aot.c — Compare interpret vs VM vs note AOT build path.
 * Micro-benchmark of classical arithmetic loop via interpreter and VM.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/interpreter.h"
#include "../include/parser.h"
#include "../include/typecheck.h"
#include "../include/ir.h"
#include "../include/vm.h"
#include "../include/specialize.h"

static double now_sec(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

static const char *PROG =
    "int acc = 0;\n"
    "int i = 0;\n"
    "while (i < 1000) {\n"
    "  acc = acc + i;\n"
    "  i = i + 1;\n"
    "}\n"
    "print(acc);\n";

int main(void) {
    const int rounds = 50;
    double t0, t1;

    printf("=== Quanti AOT / pipeline benchmark ===\n");
    printf("Program: sum 0..999, %d rounds\n\n", rounds);

    t0 = now_sec();
    for (int r = 0; r < rounds; r++) {
        Interpreter *interp = interp_create(quanti_default_config());
        if (!interp_exec(interp, PROG)) {
            fprintf(stderr, "interpreter failed\n");
            return 1;
        }
        interp_destroy(interp);
    }
    t1 = now_sec();
    printf("Interpreter: %.4f s (%.2f us/run)\n",
           t1 - t0, (t1 - t0) * 1e6 / rounds);

    t0 = now_sec();
    for (int r = 0; r < rounds; r++) {
        Parser p;
        parser_init(&p, PROG);
        Program prog = parser_parse(&p);
        IrModule *m = ir_compile(&prog);
        specialize_module(m);
        if (!vm_execute(m, quanti_default_config())) {
            fprintf(stderr, "vm failed\n");
            return 1;
        }
        ir_free(m);
        program_free(&prog);
    }
    t1 = now_sec();
    printf("IR+VM:       %.4f s (%.2f us/run)\n",
           t1 - t0, (t1 - t0) * 1e6 / rounds);

    /* Specialize check */
    Parser p;
    parser_init(&p, PROG);
    Program prog = parser_parse(&p);
    IrModule *m = ir_compile(&prog);
    specialize_module(m);
    printf("\nSpecialization: uses_karu=%d all_classical=%d insts=%zu\n",
           m->uses_karu ? 1 : 0, m->all_classical ? 1 : 0, m->count);
    printf("AOT: use `quanti build examples/classical.qa --lto`\n");
    ir_free(m);
    program_free(&prog);
    return 0;
}
