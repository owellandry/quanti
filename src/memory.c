#include "memory.h"
#include <stdlib.h>
#include <string.h>

/* ── Helpers internos ───────────────────────────────── */

/* Busca el índice interno de un nodo por karu.id. Retorna -1 si no existe. */
static int kmem_index_of(KaruMemory *mem, uint32_t karu_id) {
    for (size_t i = 0; i < mem->count; i++) {
        if (mem->nodes[i].karu.id == karu_id)
            return (int)i;
    }
    return -1;
}

static bool kmem_grow(KaruMemory *mem) {
    size_t new_cap = mem->capacity * 2;
    if (new_cap < 16) new_cap = 16;
    KaruNode *new_nodes = realloc(mem->nodes, new_cap * sizeof(KaruNode));
    if (!new_nodes) return false;
    mem->nodes    = new_nodes;
    mem->capacity = new_cap;
    return true;
}

/* ── Ciclo de vida ──────────────────────────────────── */

KaruMemory *kmem_create(size_t initial_capacity) {
    KaruMemory *mem = calloc(1, sizeof(KaruMemory));
    if (!mem) return NULL;

    if (initial_capacity < 16) initial_capacity = 16;
    mem->nodes = calloc(initial_capacity, sizeof(KaruNode));
    if (!mem->nodes) { free(mem); return NULL; }

    mem->count    = 0;
    mem->capacity = initial_capacity;
    return mem;
}

void kmem_free(KaruMemory *mem) {
    if (!mem) return;
    for (size_t i = 0; i < mem->count; i++) {
        karu_free(&mem->nodes[i].karu);
        free(mem->nodes[i].deps);
    }
    free(mem->nodes);
    free(mem);
}

/* ── Registro ───────────────────────────────────────── */

size_t kmem_register(KaruMemory *mem, KaruByte karu) {
    if (mem->count >= mem->capacity) {
        if (!kmem_grow(mem)) return (size_t)-1;
    }

    KaruNode *node = &mem->nodes[mem->count];
    node->karu      = karu_clone(karu);
    node->karu.id   = karu.id;  /* preservar ID original */
    node->deps      = NULL;
    node->dep_count = 0;
    node->collapsed = karu_is_deterministic(karu);

    return mem->count++;
}

bool kmem_add_dependency(KaruMemory *mem, uint32_t node_id, uint32_t depends_on_id) {
    int idx = kmem_index_of(mem, node_id);
    if (idx < 0) return false;

    /* Verificar que el target existe */
    if (kmem_index_of(mem, depends_on_id) < 0) return false;

    KaruNode *node = &mem->nodes[idx];

    /* Evitar duplicados */
    for (size_t i = 0; i < node->dep_count; i++) {
        if (node->deps[i] == depends_on_id) return true;
    }

    uint32_t *new_deps = realloc(node->deps, (node->dep_count + 1) * sizeof(uint32_t));
    if (!new_deps) return false;

    new_deps[node->dep_count] = depends_on_id;
    node->deps = new_deps;
    node->dep_count++;
    return true;
}

/* ── Consultas ──────────────────────────────────────── */

KaruNode *kmem_find(KaruMemory *mem, uint32_t karu_id) {
    int idx = kmem_index_of(mem, karu_id);
    if (idx < 0) return NULL;
    return &mem->nodes[idx];
}

uint32_t *kmem_get_dependents(KaruMemory *mem, uint32_t karu_id, size_t *out_count) {
    *out_count = 0;

    /* Contar dependientes: nodos cuyo array deps contiene karu_id */
    size_t cap = 8;
    uint32_t *result = malloc(cap * sizeof(uint32_t));
    if (!result) return NULL;

    for (size_t i = 0; i < mem->count; i++) {
        KaruNode *node = &mem->nodes[i];
        for (size_t j = 0; j < node->dep_count; j++) {
            if (node->deps[j] == karu_id) {
                if (*out_count >= cap) {
                    cap *= 2;
                    uint32_t *tmp = realloc(result, cap * sizeof(uint32_t));
                    if (!tmp) { free(result); *out_count = 0; return NULL; }
                    result = tmp;
                }
                result[*out_count] = node->karu.id;
                (*out_count)++;
                break;
            }
        }
    }

    return result;
}

uint32_t *kmem_get_cascade(KaruMemory *mem, uint32_t karu_id, size_t *out_count) {
    *out_count = 0;

    /* BFS sobre dependientes transitivos */
    size_t queue_cap = 16;
    uint32_t *queue = malloc(queue_cap * sizeof(uint32_t));
    if (!queue) return NULL;

    size_t result_cap = 16;
    uint32_t *result = malloc(result_cap * sizeof(uint32_t));
    if (!result) { free(queue); return NULL; }

    size_t queue_head = 0, queue_tail = 0;

    /* Seed: dependientes directos del nodo inicial */
    size_t direct_count = 0;
    uint32_t *directs = kmem_get_dependents(mem, karu_id, &direct_count);
    for (size_t i = 0; i < direct_count; i++) {
        if (queue_tail >= queue_cap) {
            queue_cap *= 2;
            uint32_t *tmp = realloc(queue, queue_cap * sizeof(uint32_t));
            if (!tmp) { free(queue); free(result); free(directs); *out_count = 0; return NULL; }
            queue = tmp;
        }
        queue[queue_tail++] = directs[i];
    }
    free(directs);

    while (queue_head < queue_tail) {
        uint32_t current = queue[queue_head++];

        /* ¿Ya lo visitamos? */
        bool seen = false;
        for (size_t i = 0; i < *out_count; i++) {
            if (result[i] == current) { seen = true; break; }
        }
        if (seen) continue;

        /* Agregar a resultado */
        if (*out_count >= result_cap) {
            result_cap *= 2;
            uint32_t *tmp = realloc(result, result_cap * sizeof(uint32_t));
            if (!tmp) { free(queue); free(result); *out_count = 0; return NULL; }
            result = tmp;
        }
        result[*out_count] = current;
        (*out_count)++;

        /* Encolar dependientes de current */
        size_t sub_count = 0;
        uint32_t *subs = kmem_get_dependents(mem, current, &sub_count);
        for (size_t i = 0; i < sub_count; i++) {
            if (queue_tail >= queue_cap) {
                queue_cap *= 2;
                uint32_t *tmp = realloc(queue, queue_cap * sizeof(uint32_t));
                if (!tmp) { free(queue); free(result); free(subs); *out_count = 0; return NULL; }
                queue = tmp;
            }
            queue[queue_tail++] = subs[i];
        }
        free(subs);
    }

    free(queue);
    return result;
}

/* ── Estado ─────────────────────────────────────────── */

bool kmem_mark_collapsed(KaruMemory *mem, uint32_t karu_id) {
    KaruNode *node = kmem_find(mem, karu_id);
    if (!node) return false;
    node->collapsed = true;
    return true;
}

bool kmem_deps_resolved(KaruMemory *mem, uint32_t karu_id) {
    KaruNode *node = kmem_find(mem, karu_id);
    if (!node) return false;

    for (size_t i = 0; i < node->dep_count; i++) {
        KaruNode *dep = kmem_find(mem, node->deps[i]);
        if (!dep || !dep->collapsed) return false;
    }
    return true;
}

/* ── Estadísticas ───────────────────────────────────── */

size_t kmem_node_count(const KaruMemory *mem) {
    return mem ? mem->count : 0;
}

size_t kmem_edge_count(const KaruMemory *mem) {
    if (!mem) return 0;
    size_t edges = 0;
    for (size_t i = 0; i < mem->count; i++)
        edges += mem->nodes[i].dep_count;
    return edges;
}
