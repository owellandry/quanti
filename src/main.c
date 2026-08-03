#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"
#include "parser.h"
#include "typecheck.h"
#include "ir.h"
#include "vm.h"
#include "codegen.h"
#include "specialize.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }

    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void print_usage(void) {
    printf("Quanti — Multi-State Runtime Programming\n");
    printf("\n");
    printf("Usage:\n");
    printf("  quanti <file.qa>              Interpret a .qa program (tree-walk)\n");
    printf("  quanti run <file.qa>          Same as interpret\n");
    printf("  quanti vm <file.qa>           Compile to IR and execute on bytecode VM\n");
    printf("  quanti build <file.qa> [-o out.exe] [--lto]\n");
    printf("                                AOT compile to native binary via C + libquanti\n");
    printf("  quanti ir <file.qa>           Dump Quenti IR disassembly\n");
    printf("  quanti --version              Show version\n");
    printf("  quanti --help                 Show this help\n");
    printf("\n");
    printf("Example:\n");
    printf("  quanti examples/demo.qa\n");
    printf("  quanti build examples/demo.qa -o build/demo.exe\n");
}

static int cmd_interpret(const char *path) {
    char *source = read_file(path);
    if (!source) return 1;

    Interpreter *interp = interp_create(quanti_default_config());
    if (!interp) {
        fprintf(stderr, "Error: failed to initialize runtime\n");
        free(source);
        return 1;
    }

    bool ok = interp_exec(interp, source);
    interp_destroy(interp);
    free(source);
    return ok ? 0 : 1;
}

static IrModule *compile_source_to_ir(const char *source, Program *out_prog) {
    Parser p;
    parser_init(&p, source);
    *out_prog = parser_parse(&p);
    if (p.had_error) {
        fprintf(stderr, "Parse error: %s\n", p.error_msg);
        program_free(out_prog);
        return NULL;
    }

    TypeCheckResult tc = typecheck_program(out_prog);
    if (tc.had_error) {
        fprintf(stderr, "Type error: %s\n", tc.error_msg);
        program_free(out_prog);
        return NULL;
    }

    IrModule *m = ir_compile(out_prog);
    if (!m || m->had_error) {
        fprintf(stderr, "IR error: %s\n", m ? m->error_msg : "compile failed");
        if (m) ir_free(m);
        program_free(out_prog);
        return NULL;
    }
    specialize_module(m);
    return m;
}

static int cmd_vm(const char *path) {
    char *source = read_file(path);
    if (!source) return 1;

    Program prog = {0};
    IrModule *m = compile_source_to_ir(source, &prog);
    free(source);
    if (!m) return 1;

    bool ok = vm_execute(m, quanti_default_config());
    ir_free(m);
    program_free(&prog);
    return ok ? 0 : 1;
}

static int cmd_ir_dump(const char *path) {
    char *source = read_file(path);
    if (!source) return 1;

    Program prog = {0};
    IrModule *m = compile_source_to_ir(source, &prog);
    free(source);
    if (!m) return 1;

    ir_disasm(m, stdout);
    printf("\n; uses_karu=%d all_classical=%d locals=%zu funcs=%zu\n",
           m->uses_karu ? 1 : 0, m->all_classical ? 1 : 0,
           m->local_count, m->func_count);

    ir_free(m);
    program_free(&prog);
    return 0;
}

static int cmd_build(int argc, char **argv) {
    /* argv[0]=build, argv[1]=file.qa, optional -o out --lto */
    if (argc < 2) {
        fprintf(stderr, "Error: quanti build requires a .qa file\n");
        return 1;
    }

    const char *qa_path = argv[1];
    char out_buf[512];
    const char *out_exe = NULL;
    bool use_lto = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--lto") == 0) {
            use_lto = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_exe = argv[++i];
        } else {
            fprintf(stderr, "Unknown build option: %s\n", argv[i]);
            return 1;
        }
    }

    if (!out_exe) {
        /* Derive build/<basename>.exe */
        const char *base = qa_path;
        const char *slash = strrchr(qa_path, '/');
        const char *bslash = strrchr(qa_path, '\\');
        if (slash && slash > base) base = slash + 1;
        if (bslash && bslash > base) base = bslash + 1;
        snprintf(out_buf, sizeof(out_buf), "build/%s", base);
        char *dot = strrchr(out_buf, '.');
        if (dot) strcpy(dot, ".exe");
        else strcat(out_buf, ".exe");
        out_exe = out_buf;
    }

    int rc = codegen_build(qa_path, out_exe, use_lto);
    if (rc == 0)
        printf("Built %s%s\n", out_exe, use_lto ? " (LTO)" : "");
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("quanti 0.2.0\n");
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        return cmd_interpret(argv[2]);
    }

    if (strcmp(argv[1], "vm") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        return cmd_vm(argv[2]);
    }

    if (strcmp(argv[1], "ir") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        return cmd_ir_dump(argv[2]);
    }

    if (strcmp(argv[1], "build") == 0) {
        return cmd_build(argc - 1, argv + 1);
    }

    /* Default: interpret file.qa */
    return cmd_interpret(argv[1]);
}
