#ifndef QUANTI_PRUNER_H
#define QUANTI_PRUNER_H

#include "branch.h"
#include <stddef.h>

/*
 * Adaptive Pruner — Poda inteligente de ramas.
 *
 * Estrategias:
 *   1. Por umbral: ramas con peso < threshold son eliminadas
 *   2. Por presión: si branch_count > max, podar las de menor peso
 *   3. Renormalización: después de podar, redistribuir pesos
 */

typedef struct {
    double   weight_threshold;   /* peso mínimo para sobrevivir */
    size_t   max_active;         /* máximo de ramas activas antes de forzar poda */
    bool     renormalize;        /* renormalizar pesos después de poda */
} PrunerConfig;

/* Configuración por defecto */
PrunerConfig pruner_default_config(void);

/* Poda adaptativa: aplica threshold + presión si excede max_active.
 * Retorna número de ramas podadas. */
size_t pruner_run(BranchManager *mgr, PrunerConfig config);

/* Renormaliza pesos de ramas activas para que sumen 1.0 */
void pruner_renormalize(BranchManager *mgr);

#endif /* QUANTI_PRUNER_H */
