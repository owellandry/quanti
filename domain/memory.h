#ifndef QUANTI_MEMORY_H
#define QUANTI_MEMORY_H

#include "karubyte.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * KaruMemory — DAG de dependencias entre KaruBytes.
 *
 * Responsabilidades:
 *   - Registrar KaruBytes y sus dependencias (quién depende de quién)
 *   - Permitir consultar dependientes de un nodo (para colapso propagante)
 *   - Compartir estado de solo lectura entre ramas (shared DAG)
 *   - Tracking de nodos persistentes (@persistent)
 *
 * El DAG es acíclico por construcción: un KaruByte solo puede depender
 * de KaruBytes creados antes que él.
 */

/* ── Nodo del DAG ───────────────────────────────────── */

typedef struct KaruNode {
    KaruByte       karu;          /* el valor multiestado */
    uint32_t      *deps;          /* IDs de los que dependo (owned) */
    size_t         dep_count;     /* número de dependencias */
    bool           collapsed;     /* ya fue colapsado? */
} KaruNode;

/* ── KaruMemory ─────────────────────────────────────── */

typedef struct {
    KaruNode  *nodes;       /* array dinámico de nodos (owned) */
    size_t     count;        /* nodos actuales */
    size_t     capacity;     /* capacidad del array */
} KaruMemory;

/* ── Ciclo de vida ──────────────────────────────────── */

KaruMemory *kmem_create(size_t initial_capacity);
void        kmem_free(KaruMemory *mem);

/* ── Registro de nodos ──────────────────────────────── */

/* Registra un KaruByte en el DAG. Retorna su índice interno.
 * El KaruMemory toma ownership del KaruByte (lo copia internamente). */
size_t kmem_register(KaruMemory *mem, KaruByte karu);

/* Registra una dependencia: `node_id` depende de `depends_on_id`.
 * Ambos deben ser IDs de KaruByte (karu.id), no índices. */
bool kmem_add_dependency(KaruMemory *mem, uint32_t node_id, uint32_t depends_on_id);

/* ── Consultas ──────────────────────────────────────── */

/* Busca un nodo por su KaruByte ID. Retorna NULL si no existe. */
KaruNode *kmem_find(KaruMemory *mem, uint32_t karu_id);

/* Retorna los IDs de todos los nodos que dependen de `karu_id`
 * (dependientes directos — para propagación de colapso).
 * El caller debe liberar el array retornado con free().
 * `out_count` recibe el número de dependientes. */
uint32_t *kmem_get_dependents(KaruMemory *mem, uint32_t karu_id, size_t *out_count);

/* Retorna todos los IDs alcanzables desde `karu_id` siguiendo
 * el grafo de dependientes (transitive closure — cascada completa).
 * El caller debe liberar el array retornado con free(). */
uint32_t *kmem_get_cascade(KaruMemory *mem, uint32_t karu_id, size_t *out_count);

/* ── Estado ─────────────────────────────────────────── */

/* Marca un nodo como colapsado */
bool kmem_mark_collapsed(KaruMemory *mem, uint32_t karu_id);

/* ¿Todos los nodos de los que dependo ya fueron colapsados? */
bool kmem_deps_resolved(KaruMemory *mem, uint32_t karu_id);

/* ── Estadísticas ───────────────────────────────────── */

size_t kmem_node_count(const KaruMemory *mem);
size_t kmem_edge_count(const KaruMemory *mem);

#endif /* QUANTI_MEMORY_H */
