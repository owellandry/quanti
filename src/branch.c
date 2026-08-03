#include "branch.h"
#include "distribution.h"
#include <stdlib.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────── */

static bool branch_grow(BranchManager *mgr) {
    size_t new_cap = mgr->capacity * 2;
    Branch *new_branches = realloc(mgr->branches, new_cap * sizeof(Branch));
    if (!new_branches) return false;
    mgr->branches = new_branches;
    mgr->capacity = new_cap;
    return true;
}

static Branch *branch_alloc(BranchManager *mgr, double weight) {
    if (mgr->count >= mgr->capacity) {
        if (!branch_grow(mgr)) return NULL;
    }

    Branch *b = &mgr->branches[mgr->count];
    memset(b, 0, sizeof(Branch));
    b->id         = ++mgr->next_id;
    b->weight     = weight;
    b->active     = true;
    b->locals     = NULL;
    b->local_count = 0;
    b->local_cap  = 0;
    mgr->count++;
    return b;
}

static bool branch_locals_grow(Branch *b) {
    size_t new_cap = (b->local_cap < 8) ? 8 : b->local_cap * 2;
    BranchVar *new_locals = realloc(b->locals, new_cap * sizeof(BranchVar));
    if (!new_locals) return false;
    b->locals    = new_locals;
    b->local_cap = new_cap;
    return true;
}

/* ── Ciclo de vida ──────────────────────────────────── */

BranchManager *branch_create(size_t max_branches, double prune_threshold) {
    BranchManager *mgr = calloc(1, sizeof(BranchManager));
    if (!mgr) return NULL;

    mgr->capacity        = 32;
    mgr->branches        = calloc(mgr->capacity, sizeof(Branch));
    mgr->count           = 0;
    mgr->max_branches    = max_branches;
    mgr->prune_threshold = prune_threshold;
    mgr->next_id         = 0;

    if (!mgr->branches) { free(mgr); return NULL; }
    return mgr;
}

void branch_free(BranchManager *mgr) {
    if (!mgr) return;
    for (size_t i = 0; i < mgr->count; i++) {
        Branch *b = &mgr->branches[i];
        for (size_t j = 0; j < b->local_count; j++)
            karu_free(&b->locals[j].value);
        free(b->locals);
    }
    free(mgr->branches);
    free(mgr);
}

void branch_clear(BranchManager *mgr) {
    if (!mgr) return;
    for (size_t i = 0; i < mgr->count; i++) {
        Branch *b = &mgr->branches[i];
        for (size_t j = 0; j < b->local_count; j++)
            karu_free(&b->locals[j].value);
        free(b->locals);
        b->locals = NULL;
        b->local_count = 0;
        b->local_cap = 0;
    }
    mgr->count = 0;
}

/* ── Fork ───────────────────────────────────────────── */

size_t branch_fork(BranchManager *mgr, KaruByte source) {
    if (!mgr) return 0;

    size_t created = 0;

    switch (source.state) {
    case KARU_SUPER: {
        /* K → 2 ramas: FALSE y TRUE, peso 0.5 cada una */
        size_t active = branch_active_count(mgr);
        if (active + 2 > mgr->max_branches) return 0;

        Branch *b0 = branch_alloc(mgr, 0.5);
        if (!b0) return 0;
        branch_set_local(mgr, b0->id, source.id, karu_false());
        created++;

        Branch *b1 = branch_alloc(mgr, 0.5);
        if (!b1) return created;
        branch_set_local(mgr, b1->id, source.id, karu_true());
        created++;
        break;
    }

    case KARU_PROB: {
        if (!source.dist || source.dist->type != DIST_DISCRETE) {
            /* Solo discretas generan ramas explícitas por ahora */
            /* Para continuas: 2 ramas (>0.5 y <=0.5) */
            size_t active = branch_active_count(mgr);
            if (active + 2 > mgr->max_branches) return 0;

            Branch *bl = branch_alloc(mgr, 0.5);
            if (!bl) return 0;
            branch_set_local(mgr, bl->id, source.id, karu_false());
            created++;

            Branch *bh = branch_alloc(mgr, 0.5);
            if (!bh) return created;
            branch_set_local(mgr, bh->id, source.id, karu_true());
            created++;
            break;
        }

        size_t n = source.dist->params.discrete.n;
        size_t active = branch_active_count(mgr);
        if (active + n > mgr->max_branches) return 0;

        for (size_t i = 0; i < n; i++) {
            double w = source.dist->params.discrete.probs[i];
            /* Saltar ramas con peso bajo el umbral */
            if (w < mgr->prune_threshold) continue;

            Branch *b = branch_alloc(mgr, w);
            if (!b) return created;
            /* Rama i: valor determinista basado en índice */
            KaruByte val = (i == 0) ? karu_false() : karu_true();
            branch_set_local(mgr, b->id, source.id, val);
            created++;
        }
        break;
    }

    default:
        /* Tipos deterministas o indefinidos no generan fork */
        return 0;
    }

    return created;
}

/* ── Estado local ───────────────────────────────────── */

bool branch_set_local(BranchManager *mgr, uint32_t branch_id,
                      uint32_t karu_id, KaruByte value) {
    Branch *b = branch_get(mgr, branch_id);
    if (!b) return false;

    /* Buscar si ya existe */
    for (size_t i = 0; i < b->local_count; i++) {
        if (b->locals[i].karu_id == karu_id) {
            karu_free(&b->locals[i].value);
            b->locals[i].value = karu_clone(value);
            return true;
        }
    }

    /* Agregar nuevo */
    if (b->local_count >= b->local_cap) {
        if (!branch_locals_grow(b)) return false;
    }

    b->locals[b->local_count].karu_id = karu_id;
    b->locals[b->local_count].value   = karu_clone(value);
    b->local_count++;
    return true;
}

KaruByte *branch_get_local(BranchManager *mgr, uint32_t branch_id, uint32_t karu_id) {
    Branch *b = branch_get(mgr, branch_id);
    if (!b) return NULL;

    for (size_t i = 0; i < b->local_count; i++) {
        if (b->locals[i].karu_id == karu_id)
            return &b->locals[i].value;
    }
    return NULL;
}

/* ── Merge ──────────────────────────────────────────── */

KaruByte branch_merge(BranchManager *mgr, uint32_t karu_id) {
    if (!mgr) return karu_undef();

    /* Recolectar valores únicos de ramas activas */
    bool has_false = false, has_true = false;
    int  active_with_var = 0;

    for (size_t i = 0; i < mgr->count; i++) {
        Branch *b = &mgr->branches[i];
        if (!b->active) continue;

        KaruByte *val = branch_get_local(mgr, b->id, karu_id);
        if (!val) continue;

        active_with_var++;
        if (val->state == KARU_FALSE) has_false = true;
        if (val->state == KARU_TRUE)  has_true = true;
    }

    if (active_with_var == 0) return karu_undef();
    if (has_false && has_true) return karu_super();   /* divergencia → superposición */
    if (has_true)              return karu_true();     /* todas TRUE */
    if (has_false)             return karu_false();    /* todas FALSE */

    return karu_undef();
}

/* ── Poda ───────────────────────────────────────────── */

size_t branch_prune(BranchManager *mgr) {
    if (!mgr) return 0;

    size_t pruned = 0;
    for (size_t i = 0; i < mgr->count; i++) {
        Branch *b = &mgr->branches[i];
        if (!b->active) continue;
        if (b->weight < mgr->prune_threshold) {
            b->active = false;
            pruned++;
        }
    }
    return pruned;
}

/* ── Consulta ───────────────────────────────────────── */

size_t branch_active_count(const BranchManager *mgr) {
    if (!mgr) return 0;
    size_t count = 0;
    for (size_t i = 0; i < mgr->count; i++) {
        if (mgr->branches[i].active) count++;
    }
    return count;
}

Branch *branch_get(BranchManager *mgr, uint32_t branch_id) {
    if (!mgr) return NULL;
    for (size_t i = 0; i < mgr->count; i++) {
        if (mgr->branches[i].id == branch_id)
            return &mgr->branches[i];
    }
    return NULL;
}
