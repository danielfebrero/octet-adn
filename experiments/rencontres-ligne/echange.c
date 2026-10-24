/* Rencontres et transfert horizontal, independants de SPLIT et du plafond.
 * Aucun nouveau-ne, deplacement, don d'energie ou octet gratuit n'est cree.
 * Deux voisins echangent leurs octets aux memes adresses, dans le soma et
 * le germen separement. Leurs longueurs et leurs compteurs de naissance
 * restent identiques. Le moteur appelant recalcule ses caches de genotype.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "echange.h"

void echange_init(EchangeState *S, uint64_t seed) {
    memset(S, 0, sizeof *S);
    rng_seed(&S->rng, seed);
}

static int echange_near(const World *w, const Creature *a, const Creature *b) {
    int dx = abs(a->x - b->x), dy = abs(a->y - b->y);
    if (dx > w->cfg.w - dx) dx = w->cfg.w - dx;
    if (dy > w->cfg.h - dy) dy = w->cfg.h - dy;
    return dx <= 1 && dy <= 1;
}

static int history_find(const EchangeState *S, int id) {
    for (int i = 0; i < S->n_history; i++) if (S->ids[i] == id) return i;
    return -1;
}

static int cooling(const EchangeState *S, int id, long tick, int cooldown) {
    if (cooldown <= 0) return 0;
    int i = history_find(S, id);
    return i >= 0 && (tick < S->last_exchange[i]
                     || tick - S->last_exchange[i] < cooldown);
}

static void history_record(EchangeState *S, int id, long tick) {
    int i = history_find(S, id);
    if (i < 0) {
        if (S->n_history < CMAX) i = S->n_history++;
        else {
            i = 0;
            for (int j = 1; j < CMAX; j++)
                if (S->last_exchange[j] < S->last_exchange[i]) i = j;
        }
        S->ids[i] = id;
    }
    S->last_exchange[i] = tick;
}

/* Choisir une adresse ou au moins un des deux stocks differe. Pas de
 * substitution : la somme des alleles de la paire est conservee exactement.
 * Au plus huit adresses contigues changent, sans decaler les immediats.
 */
static int reciprocal(Creature *a, Creature *b, Rng *r) {
    int n = a->len;
    if (b->len < n) n = b->len;
    if (a->germ_len < n) n = a->germ_len;
    if (b->germ_len < n) n = b->germ_len;
    if (n <= 0 || n > MAX_LEN) return 0;
    int locus = -1, differences = 0;
    for (int i = 0; i < n; i++) {
        if (a->dna[i] == b->dna[i] && a->germ[i] == b->germ[i]) continue;
        if (rng_below(r, (uint32_t)++differences) == 0) locus = i;
    }
    if (locus < 0) return 0;
    int hi = n - locus < 8 ? n - locus : 8;
    int length = 1 + (int)rng_below(r, (uint32_t)hi), changed = 0;
    for (int i = locus; i < locus + length; i++) {
        uint8_t t = a->dna[i];
        if (t != b->dna[i]) {
            a->dna[i] = b->dna[i]; b->dna[i] = t;
            /* dirty suit les STORE, pas les transferts horizontaux. */
            a->dirty[i >> 3] &= (uint8_t)~(1u << (i & 7));
            b->dirty[i >> 3] &= (uint8_t)~(1u << (i & 7));
            changed += 2;
        }
        t = a->germ[i];
        if (t != b->germ[i]) { a->germ[i] = b->germ[i]; b->germ[i] = t; changed += 2; }
    }
    return changed;
}

void echange_step(World *w, const EchangeConfig *C, EchangeState *S) {
    /* Ce retour precede toute lecture d'etat et tout tirage. */
    if (!C->enabled) return;
    int period = C->period > 0 ? C->period : 1;
    if (w->t % period != 0) return;
    int order[CMAX], blocked[CMAX];
    uint8_t used[CMAX] = { 0 };
    int n = 0;
    for (int i = 0; i < w->ncs && i < CMAX; i++) {
        if (w->cs[i]->e <= 0) continue;
        order[n++] = i;
        blocked[i] = cooling(S, w->cs[i]->id, w->t, C->cooldown);
    }
    S->passages++;
    /* Toutes les paires geometriques uniques, avant choix ou refus. intra
     * et inter partitionnent contacts. cooldown_refused compte les paires
     * qui ne peuvent pas etre choisies a cause du repos d'un partenaire.
     */
    for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) {
        Creature *x = w->cs[order[a]], *y = w->cs[order[b]];
        if (x->id == y->id || !echange_near(w, x, y)) continue;
        S->contacts++;
        if (x->species == y->species) S->intra++; else S->inter++;
        if (blocked[order[a]] || blocked[order[b]]) S->cooldown_refused++;
    }
    /* Ordre et partenaire uniformises, sans utiliser le RNG du monde. */
    for (int i = n - 1; i > 0; i--) {
        int j = (int)rng_below(&S->rng, (uint32_t)(i + 1)), t = order[i];
        order[i] = order[j]; order[j] = t;
    }
    int cost = C->cost > 0 ? C->cost : 0;
    for (int a = 0; a < n; a++) {
        int ai = order[a]; Creature *x = w->cs[ai];
        if (used[ai] || blocked[ai]) continue;
        int bi = -1, candidates = 0;
        for (int b = a + 1; b < n; b++) {
            int k = order[b]; Creature *y = w->cs[k];
            if (used[k] || blocked[k] || x->id == y->id || !echange_near(w, x, y)) continue;
            if (rng_below(&S->rng, (uint32_t)++candidates) == 0) bi = k;
        }
        if (bi < 0) continue;
        Creature *y = w->cs[bi];
        used[ai] = used[bi] = 1;
        S->attempts++;
        /* Ne jamais tuer un partenaire par le cout d'echange. */
        if (x->e <= cost || y->e <= cost) { S->energy_refused++; continue; }
        int changed = reciprocal(x, y, &S->rng);
        if (!changed) continue;
        x->e -= cost; y->e -= cost;
        S->changes++; S->bytes_changed += changed;
        history_record(S, x->id, w->t); history_record(S, y->id, w->t);
    }
}

/* Scenarios construits : ils testent les nouvelles possibilites physiques,
 * sans attribuer a l'evolution le comportement des fondateurs de test.
 */
static World *test_pair(int same) {
    Rng r; rng_seed(&r, 19);
    WorldConfig C = cfg_default(); C.w = C.h = 8; C.max_pop = 2;
    World *w = world_new(NULL, 0, &r, &C, NULL);
    uint8_t a[16], b[16];
    memset(a, OP_EAT, sizeof a); memset(b, same ? OP_EAT : OP_SENSE, sizeof b);
    w->cs[0] = creature_new(1, 1, a, sizeof a, 100, 10, 2, -1);
    w->cs[1] = creature_new(2, 1, b, sizeof b, 100, 20, 3, -1);
    w->ncs = 2; w->nid = 21;
    w->occ[1][1] = w->occ[1][2] = 1;
    w->st.born = 2;
    /* Pas de world_step dans cette sonde ; rng pointe normalement vers
     * l'etat de l'ile et n'est jamais lu par echange_step. */
    w->rng = NULL;
    return w;
}

#define ECHECK(label, condition) do { \
    int good = !!(condition); \
    printf("selftest echange : %s %s\n", label, good ? "OK" : "ECHEC"); \
    ok &= good; \
} while (0)

int echange_selftest(void) {
    int ok = 1;
    EchangeConfig C = { 1, 1, 7, 5 };
    EchangeState S, before;
    echange_init(&S, 91);
    World *w = test_pair(0);
    Creature a = *w->cs[0], b = *w->cs[1];
    long born = w->st.born, dead = w->st.dead;
    echange_step(w, &C, &S);
    int soma = memcmp(a.dna, w->cs[0]->dna, 16) && memcmp(b.dna, w->cs[1]->dna, 16);
    int germ = memcmp(a.germ, w->cs[0]->germ, 16) && memcmp(b.germ, w->cs[1]->germ, 16);
    int conserved = 1;
    for (int i = 0; i < 16; i++) {
        conserved &= a.dna[i] + b.dna[i] == w->cs[0]->dna[i] + w->cs[1]->dna[i];
        conserved &= a.germ[i] + b.germ[i] == w->cs[0]->germ[i] + w->cs[1]->germ[i];
    }
    ECHECK("plafond plein, zero mort, soma et germen changent",
           S.contacts == 1 && S.inter == 1 && S.intra == 0 && S.changes == 1 && soma && germ
           && w->ncs == w->cfg.max_pop && w->nnb == 0 && w->st.born == born && w->st.dead == dead
           && w->cs[0]->splits == 0 && w->cs[1]->splits == 0 && conserved
           && w->cs[0]->len == 16 && w->cs[0]->germ_len == 16);
    ECHECK("cout reel sur les deux partenaires", w->cs[0]->e == 93 && w->cs[1]->e == 93);
    long changes = S.changes; w->t = 1;
    echange_step(w, &C, &S);
    ECHECK("cooldown individuel interdit un nouveau transfert", S.changes == changes && S.cooldown_refused == 1);
    w->t = 5; echange_step(w, &C, &S);
    ECHECK("cooldown expire a la borne", S.changes == changes + 1);
    world_free(w);

    w = test_pair(0); echange_init(&S, 91);
    w->cs[0]->x = 0; w->cs[0]->y = 0; w->cs[1]->x = 7; w->cs[1]->y = 7;
    echange_step(w, &C, &S);
    ECHECK("rencontre diagonale a travers les deux bords du tore", S.contacts == 1 && S.changes == 1);
    world_free(w);

    w = test_pair(0); echange_init(&S, 91);
    w->cs[1]->x = 5; w->cs[1]->y = 5;
    echange_step(w, &C, &S);
    ECHECK("aucun transfert entre creatures eloignees", S.contacts == 0 && S.attempts == 0 && S.changes == 0);
    w->cs[1]->x = 2; w->cs[1]->y = 1; w->cs[1]->id = w->cs[0]->id;
    echange_step(w, &C, &S);
    ECHECK("deux entrees du meme id ne sont pas partenaires", S.contacts == 0 && S.attempts == 0);
    w->cs[1]->id = 20; w->cs[1]->e = 0;
    echange_step(w, &C, &S);
    ECHECK("un partenaire mort ne compte pas comme rencontre", S.contacts == 0 && S.attempts == 0);
    world_free(w);

    w = test_pair(0); echange_init(&S, 91); w->cfg.max_pop = 4;
    for (int i = 2; i < 4; i++) {
        uint8_t dna[16]; memset(dna, i == 2 ? OP_ATTACK : OP_YIELD, sizeof dna);
        w->cs[i] = creature_new(1, 2, dna, sizeof dna, 100, 30 + i, 2, -1);
    }
    w->ncs = 4; w->st.born = 4;
    echange_step(w, &C, &S);
    int once = S.contacts == 6 && S.intra + S.inter == 6 && S.attempts == 2 && S.changes == 2;
    for (int i = 0; i < 4; i++) once &= w->cs[i]->e == 93;
    ECHECK("chaque paire compte une fois, chaque individu paie au plus une fois", once);
    world_free(w);

    w = test_pair(0); echange_init(&S, 91); w->cs[1]->e = C.cost;
    a = *w->cs[0]; b = *w->cs[1];
    echange_step(w, &C, &S);
    ECHECK("energie insuffisante refuse sans effet", S.energy_refused == 1 && S.changes == 0
           && !memcmp(&a, w->cs[0], sizeof a) && !memcmp(&b, w->cs[1], sizeof b));
    world_free(w);

    w = test_pair(1); echange_init(&S, 91); a = *w->cs[0]; b = *w->cs[1];
    echange_step(w, &C, &S);
    ECHECK("genomes identiques sans changement ni paiement", S.attempts == 1 && S.changes == 0
           && S.bytes_changed == 0 && S.n_history == 0
           && !memcmp(&a, w->cs[0], sizeof a) && !memcmp(&b, w->cs[1], sizeof b));
    world_free(w);

    w = test_pair(0); echange_init(&S, 91); before = S;
    a = *w->cs[0]; b = *w->cs[1]; C.enabled = 0;
    echange_step(w, &C, &S);
    ECHECK("drapeau eteint, etat et RNG strictement identiques", !memcmp(&S, &before, sizeof S)
           && !memcmp(&a, w->cs[0], sizeof a) && !memcmp(&b, w->cs[1], sizeof b));
    C.enabled = 1; C.period = 4; w->t = 3;
    echange_step(w, &C, &S);
    ECHECK("entre passages, aucun tirage", !memcmp(&S, &before, sizeof S));
    C.period = 1; world_free(w);

    World *v = test_pair(0); w = test_pair(0);
    EchangeState T; echange_init(&S, 718); echange_init(&T, 718);
    for (long t = 0; t < 30; t++) { w->t = v->t = t; echange_step(w, &C, &S); echange_step(v, &C, &T); }
    ECHECK("determinisme de l'etat et des deux stocks", !memcmp(&S, &T, sizeof S)
           && !memcmp(w->cs[0], v->cs[0], sizeof(Creature)) && !memcmp(w->cs[1], v->cs[1], sizeof(Creature)));
    world_free(w); world_free(v);
    return ok ? 0 : 1;
}
#undef ECHECK
