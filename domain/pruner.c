#include "pruner.h"
#include <stdlib.h>
#include <float.h>

PrunerConfig pruner_default_config(void) {
    return (PrunerConfig){
        .weight_threshold = 0.01,
        .max_active       = 64,
        .renormalize      = true
    };
}

size_t pruner_run(BranchManager *mgr, PrunerConfig config) {
    if (!mgr) return 0;

    size_t pruned = 0;

    /* Paso 1: podar por umbral de peso */
    for (size_t i = 0; i < mgr->count; i++) {
        Branch *b = &mgr->branches[i];
        if (!b->active) continue;
        if (b->weight < config.weight_threshold) {
            b->active = false;
            pruned++;
        }
    }

    /* Paso 2: si aún hay demasiadas, podar las de menor peso */
    size_t active = branch_active_count(mgr);
    if (active > config.max_active) {
        /* Recolectar índices de ramas activas ordenadas por peso */
        size_t *indices = malloc(active * sizeof(size_t));
        double *weights = malloc(active * sizeof(double));
        if (indices && weights) {
            size_t idx = 0;
            for (size_t i = 0; i < mgr->count; i++) {
                if (mgr->branches[i].active) {
                    indices[idx] = i;
                    weights[idx] = mgr->branches[i].weight;
                    idx++;
                }
            }

            /* Ordenar por peso ascendente (bubble sort simple, N es pequeño) */
            for (size_t i = 0; i < idx - 1; i++) {
                for (size_t j = 0; j < idx - i - 1; j++) {
                    if (weights[j] > weights[j+1]) {
                        double tw = weights[j]; weights[j] = weights[j+1]; weights[j+1] = tw;
                        size_t ti = indices[j]; indices[j] = indices[j+1]; indices[j+1] = ti;
                    }
                }
            }

            /* Podar las de menor peso hasta llegar al máximo */
            size_t to_prune = active - config.max_active;
            for (size_t i = 0; i < to_prune && i < idx; i++) {
                mgr->branches[indices[i]].active = false;
                pruned++;
            }
        }
        free(indices);
        free(weights);
    }

    /* Paso 3: renormalizar si se configuró */
    if (config.renormalize && pruned > 0) {
        pruner_renormalize(mgr);
    }

    return pruned;
}

void pruner_renormalize(BranchManager *mgr) {
    if (!mgr) return;

    double sum = 0.0;
    for (size_t i = 0; i < mgr->count; i++) {
        if (mgr->branches[i].active)
            sum += mgr->branches[i].weight;
    }

    if (sum <= 0.0) return;

    for (size_t i = 0; i < mgr->count; i++) {
        if (mgr->branches[i].active)
            mgr->branches[i].weight /= sum;
    }
}
