#include "collapse.h"
#include "distribution.h"
#include <stdlib.h>

/* ── Colapso individual ─────────────────────────────── */

KaruByte collapse_single(KaruByte k, CollapseMode mode) {
    /* Ya determinista — no hacer nada */
    if (k.state == KARU_FALSE || k.state == KARU_TRUE)
        return karu_clone(k);

    switch (k.state) {
    case KARU_SUPER:
        /* K = ambos simultáneamente.
         * MAP  → desempate por orden → TRUE (1 > 0)
         * SAMPLE → 50/50
         * FIRST  → FALSE (primera rama = 0) */
        switch (mode) {
        case COLLAPSE_MAP:    return karu_true();
        case COLLAPSE_SAMPLE: return (dist_sample(NULL) < 0.5) ? karu_false() : karu_true();
        case COLLAPSE_FIRST:  return karu_false();
        }
        break;

    case KARU_UNDEF:
        /* Ø = indefinido. Al colapsar, se resuelve como FALSE
         * (ausencia de información = negación). */
        return karu_false();

    case KARU_PROB:
        if (!k.dist) return karu_false();

        switch (mode) {
        case COLLAPSE_MAP: {
            double val = dist_map_value(k.dist);
            /* Para discretas: index 0 → FALSE, index > 0 → TRUE
             * Para continuas: valor > 0.5 → TRUE */
            if (k.dist->type == DIST_DISCRETE)
                return (val > 0.0) ? karu_true() : karu_false();
            else
                return (val > 0.5) ? karu_true() : karu_false();
        }
        case COLLAPSE_SAMPLE: {
            double val = dist_sample(k.dist);
            if (k.dist->type == DIST_DISCRETE)
                return (val > 0.0) ? karu_true() : karu_false();
            else
                return (val > 0.5) ? karu_true() : karu_false();
        }
        case COLLAPSE_FIRST:
            return karu_false();  /* primera rama siempre = 0 */
        }
        break;

    default:
        break;
    }

    return karu_false();
}

/* ── Colapso con propagación ────────────────────────── */

size_t collapse_propagate(KaruMemory *mem, uint32_t karu_id, CollapseMode mode) {
    if (!mem) return 0;

    KaruNode *node = kmem_find(mem, karu_id);
    if (!node) return 0;

    /* Si ya está colapsado, no hacer nada */
    if (node->collapsed) return 0;

    size_t collapsed_count = 0;

    /* Colapsar el nodo raíz */
    KaruByte result = collapse_single(node->karu, mode);
    karu_free(&node->karu);
    node->karu = result;
    node->karu.id = karu_id;  /* preservar ID */
    node->collapsed = true;
    collapsed_count++;

    /* Obtener cascada transitiva */
    size_t cascade_count = 0;
    uint32_t *cascade = kmem_get_cascade(mem, karu_id, &cascade_count);

    for (size_t i = 0; i < cascade_count; i++) {
        KaruNode *dep = kmem_find(mem, cascade[i]);
        if (!dep || dep->collapsed) continue;

        /* Respetar @persistent */
        if (dep->karu.persistent) continue;

        /* Solo colapsar si todas sus dependencias ya están resueltas */
        if (!kmem_deps_resolved(mem, cascade[i])) continue;

        KaruByte dep_result = collapse_single(dep->karu, mode);
        uint32_t saved_id = dep->karu.id;
        karu_free(&dep->karu);
        dep->karu = dep_result;
        dep->karu.id = saved_id;
        dep->collapsed = true;
        collapsed_count++;
    }

    free(cascade);
    return collapsed_count;
}

/* ── Colapso forzado ────────────────────────────────── */

size_t collapse_all_non_persistent(KaruMemory *mem, CollapseMode mode) {
    if (!mem) return 0;

    size_t collapsed_count = 0;

    for (size_t i = 0; i < mem->count; i++) {
        KaruNode *node = &mem->nodes[i];
        if (node->collapsed) continue;
        if (node->karu.persistent) continue;

        KaruByte result = collapse_single(node->karu, mode);
        uint32_t saved_id = node->karu.id;
        karu_free(&node->karu);
        node->karu = result;
        node->karu.id = saved_id;
        node->collapsed = true;
        collapsed_count++;
    }

    return collapsed_count;
}

/* ── Utilidades ─────────────────────────────────────── */

const char *collapse_mode_name(CollapseMode mode) {
    switch (mode) {
    case COLLAPSE_MAP:    return "measure:map";
    case COLLAPSE_SAMPLE: return "measure:sample";
    case COLLAPSE_FIRST:  return "measure:first";
    }
    return "unknown";
}
