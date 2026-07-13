#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void print_usage(void) {
    printf("Quanti — Multi-State Runtime Programming\n");
    printf("\n");
    printf("Usage:\n");
    printf("  quanti <file.qa>        Execute a .qa program\n");
    printf("  quanti --version        Show version\n");
    printf("  quanti --help           Show this help\n");
    printf("\n");
    printf("Example:\n");
    printf("  quanti hello.qa\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("quanti 0.1.0\n");
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    }

    char *source = read_file(argv[1]);
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
