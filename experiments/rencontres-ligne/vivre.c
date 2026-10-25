/* Trois physiques de vie (voir vivre.h). Aucune n'invente un opcode ; aucune ne touche le RNG
 * du monde. Les octets hérités sont lus en fin de génome : SPLIT recopie le soma, donc la
 * mutation du moteur les fait dériver comme n'importe quel autre octet. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vivre.h"

void vivre_init(VivreState *s, uint64_t seed) {
    memset(s, 0, sizeof *s);
    rng_seed(&s->rng, seed);
}

static int near1(const World *w, const Creature *a, const Creature *b) {
    int dx = abs(a->x - b->x), dy = abs(a->y - b->y);
    if (dx > w->cfg.w - dx) dx = w->cfg.w - dx;
    if (dy > w->cfg.h - dy) dy = w->cfg.h - dy;
    return dx <= 1 && dy <= 1;
}

/* Présence d'un opcode réel (les immédiats sont sautés), comme l'ancienne porte des rendez-vous. */
static int has_op(const Creature *c, uint8_t want) {
    if (c->len <= 0 || c->len > MAX_LEN) return 0;
    for (int i = 0; i < c->len;) {
        uint8_t op = c->dna[i++];
        if (op == want) return 1;
        if (op_has_imm(op)) i++;
    }
    return 0;
}

/* Octet hérité k (1 = dernier). 0 si le génome est trop court. */
static int tail_byte(const Creature *c, int k) {
    return c->len >= k && c->len <= MAX_LEN ? c->dna[c->len - k] : 0;
}

static int shared_len(const Creature *a, const Creature *b) {
    int n = a->len;
    if (b->len < n) n = b->len;
    if (a->germ_len < n) n = a->germ_len;
    if (b->germ_len < n) n = b->germ_len;
    return n > 0 && n <= MAX_LEN ? n : 0;
}

static int hamming(const Creature *a, const Creature *b, int n) {
    int d = 0;
    for (int i = 0; i < n; i++) d += a->dna[i] != b->dna[i];
    return d;
}

/* Copie à sens unique : au plus 8 octets de la proie vers le chasseur, en bouclant modulo n.
 * Le locus est tiré parmi les adresses qui diffèrent. Soma et germen du chasseur reçoivent
 * la même valeur. La proie ne change pas d'ADN. */
static int copy_segment(Creature *x, const Creature *y, int n, Rng *r) {
    int locus = -1, diff = 0;
    for (int i = 0; i < n; i++) if (x->dna[i] != y->dna[i] && rng_below(r, (uint32_t)++diff) == 0) locus = i;
    if (locus < 0) return 0;
    int hi = n < 8 ? n : 8, length = 1 + (int)rng_below(r, (uint32_t)hi), changed = 0;
    for (int k = 0; k < length; k++) {
        int i = (locus + k) % n;
        if (x->dna[i] != y->dna[i]) {
            x->dna[i] = y->dna[i];
            x->dirty[i >> 3] &= (uint8_t)~(1u << (i & 7));
            changed++;
        }
        x->germ[i] = y->dna[i];
    }
    return changed;
}

int vivre_step(World *w, const VivreConfig *C, VivreState *S) {
    if (!C->capture && !C->parente && !C->senescence) return 0;   /* aucun tirage, aucun état */
    int changed = 0;
    if (C->senescence) {
        for (int i = 0; i < w->ncs; i++) {
            Creature *c = w->cs[i];
            if (c->e <= 0) continue;
            int max_age = C->life_base + C->life_step * tail_byte(c, 2);
            if (c->age >= max_age) { c->e = 0; S->old_deaths++; }
        }
    }
    int period = C->period > 0 ? C->period : 16;
    if ((!C->capture && !C->parente) || w->t % period) return changed;
    S->passages++;
    int order[CMAX], n = 0;
    for (int i = 0; i < w->ncs && i < CMAX; i++) if (w->cs[i]->e > 0) order[n++] = i;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)rng_below(&S->rng, (uint32_t)(i + 1)), t = order[i];
        order[i] = order[j]; order[j] = t;
    }
    if (C->capture) {
        uint8_t hunted[CMAX] = { 0 };
        for (int a = 0; a < n; a++) {
            Creature *x = w->cs[order[a]];
            if (x->e <= C->capture_cost || !has_op(x, OP_ATTACK)) continue;
            int bi = -1, cand = 0;
            for (int b = 0; b < n; b++) {
                int k = order[b]; Creature *y = w->cs[k];
                if (y == x || y->id == x->id || hunted[k] || y->e <= 0 || !near1(w, x, y)) continue;
                int m = shared_len(x, y);
                if (!m || !hamming(x, y, m)) continue;
                if (rng_below(&S->rng, (uint32_t)++cand) == 0) bi = k;
            }
            if (bi < 0) continue;
            Creature *y = w->cs[bi];
            if (y->e <= C->capture_take) { S->capture_refused++; continue; }   /* jamais tuer pour copier */
            int m = shared_len(x, y);
            S->capture_dist_sum += hamming(x, y, m);
            int got = copy_segment(x, y, m, &S->rng);
            hunted[bi] = 1;
            x->e -= C->capture_cost;
            x->e = x->e + C->capture_gain > 220 ? 220 : x->e + C->capture_gain;
            y->e -= C->capture_take;
            S->captures++; S->capture_bytes += got;
            if (got) changed = 1;
        }
    }
    if (C->parente) {
        uint8_t fed[CMAX] = { 0 };
        for (int a = 0; a < n; a++) {
            Creature *x = w->cs[order[a]];
            if (x->e <= C->give_rich || x->e <= C->give + 1) continue;
            int bi = -1, cand = 0;
            for (int b = 0; b < n; b++) {
                int k = order[b]; Creature *y = w->cs[k];
                if (y == x || y->id == x->id || fed[k] || y->e <= 0 || y->e >= C->give_poor || !near1(w, x, y)) continue;
                if (rng_below(&S->rng, (uint32_t)++cand) == 0) bi = k;
            }
            if (bi < 0) continue;
            /* La propension est l'octet hérité : rien n'oblige à donner. */
            if ((int)rng_below(&S->rng, 255) >= tail_byte(x, 1)) { S->donation_refused++; continue; }
            Creature *y = w->cs[bi];
            fed[bi] = 1;
            x->e -= C->give + 1;                       /* une unité perdue : donner coûte */
            y->e = y->e + C->give > 220 ? 220 : y->e + C->give;
            S->donations++; S->donated += C->give;
        }
    }
    return changed;
}

void vivre_sample(const World *w, VivreSample *o) {
    memset(o, 0, sizeof *o);
    double g = 0, l = 0, z = 0, at = 0, xo = 0;
    for (int i = 0; i < w->ncs; i++) {
        const Creature *c = w->cs[i];
        if (c->e <= 0) continue;
        o->n++;
        g += tail_byte(c, 1); l += tail_byte(c, 2); z += tail_byte(c, 3);
        at += has_op(c, OP_ATTACK); xo += has_op(c, OP_XOR);
    }
    if (o->n) {
        o->give_byte = g / o->n; o->life_byte = l / o->n; o->neutral_byte = z / o->n;
        o->frac_attack = at / o->n; o->frac_xor = xo / o->n;
    }
}

/* ---------------- selftest : fixtures privées, jamais un monde vivant ---------------- */
static World *pair(uint8_t fa, uint8_t fb, int ea, int eb) {
    Rng r; rng_seed(&r, 31);
    WorldConfig C = cfg_default(); C.w = C.h = 8; C.max_pop = 4;
    World *w = world_new(NULL, 0, &r, &C, NULL);
    uint8_t a[16], b[16];
    memset(a, fa, sizeof a); memset(b, fb, sizeof b);
    w->cs[0] = creature_new(1, 1, a, 16, ea, 10, 0, -1);
    w->cs[1] = creature_new(2, 1, b, 16, eb, 20, 0, -1);
    w->ncs = 2; w->occ[1][1] = w->occ[1][2] = 1;
    w->rng = NULL;                                 /* jamais lu par vivre_step */
    return w;
}

#define VCHECK(label, cond) do { int ok_ = !!(cond); printf("selftest vivre : %s %s\n", label, ok_ ? "OK" : "ECHEC"); fails += !ok_; } while (0)

int vivre_selftest(void) {
    int fails = 0;
    VivreConfig base = { 0, 0, 0, 1, 2, 4, 2, 10, 60, 40, 100, 8 };
    VivreState S, before;

    /* Éteint : aucun état, aucun tirage, aucune créature touchée. */
    World *w = pair(OP_ATTACK, OP_EAT, 100, 100);
    vivre_init(&S, 5); before = S;
    Creature a0 = *w->cs[0], b0 = *w->cs[1];
    vivre_step(w, &base, &S);
    VCHECK("drapeaux éteints, état et créatures identiques", !memcmp(&S, &before, sizeof S)
           && !memcmp(&a0, w->cs[0], sizeof a0) && !memcmp(&b0, w->cs[1], sizeof b0));
    world_free(w);

    /* Capture : le chasseur prend des octets, la proie garde son ADN et perd de l'énergie. */
    VivreConfig cap = base; cap.capture = 1;
    w = pair(OP_ATTACK, OP_EAT, 100, 100); vivre_init(&S, 5);
    b0 = *w->cs[1];
    int ch = vivre_step(w, &cap, &S);
    int took = 0; for (int i = 0; i < 16; i++) took += w->cs[0]->dna[i] == OP_EAT && w->cs[0]->germ[i] == OP_EAT;
    VCHECK("capture à sens unique, coût et gain", ch && S.captures == 1 && took >= 1 && took <= 8
           && !memcmp(b0.dna, w->cs[1]->dna, 16) && w->cs[1]->e == 96 && w->cs[0]->e == 100);
    world_free(w);
    w = pair(OP_EAT, OP_EAT + 1, 100, 100); vivre_init(&S, 5);
    vivre_step(w, &cap, &S);
    VCHECK("sans ATTACK dans le génome, pas de capture", S.captures == 0);
    world_free(w);
    w = pair(OP_ATTACK, OP_EAT, 100, 4); vivre_init(&S, 5);
    vivre_step(w, &cap, &S);
    VCHECK("une proie trop faible n'est pas tuée pour copier", S.captures == 0 && S.capture_refused == 1 && w->cs[1]->e == 4);
    world_free(w);

    /* Parenté : octet 255 = donne toujours, 0 = jamais. Énergie : −11 donneur, +10 receveur. */
    VivreConfig par = base; par.parente = 1;
    w = pair(0xFF, OP_EAT, 100, 20); vivre_init(&S, 5);
    vivre_step(w, &par, &S);
    VCHECK("propension 255 : don au voisin affamé", S.donations == 1 && w->cs[0]->e == 89 && w->cs[1]->e == 30);
    world_free(w);
    w = pair(0x00, OP_EAT, 100, 20); vivre_init(&S, 5);
    vivre_step(w, &par, &S);
    VCHECK("propension 0 : aucun don", S.donations == 0 && S.donation_refused == 1 && w->cs[1]->e == 20);
    world_free(w);
    w = pair(0xFF, OP_EAT, 100, 50); vivre_init(&S, 5);
    vivre_step(w, &par, &S);
    VCHECK("voisin pas affamé : aucun don", S.donations == 0 && S.donation_refused == 0);
    world_free(w);

    /* Sénescence : âge max = 100 + 8 × octet(len-2). Octet 0 → mort à 100, pas avant. */
    VivreConfig sen = base; sen.senescence = 1;
    w = pair(0x00, 0x00, 100, 100); vivre_init(&S, 5);
    w->cs[0]->age = 99; w->cs[1]->age = 100;
    vivre_step(w, &sen, &S);
    VCHECK("mort à l'âge hérité, pas un tick avant", w->cs[0]->e == 100 && w->cs[1]->e == 0 && S.old_deaths == 1);
    world_free(w);

    /* Déterminisme : deux mêmes fixtures, même graine, mêmes états. */
    VivreConfig all = base; all.capture = all.parente = all.senescence = 1;
    World *u = pair(OP_ATTACK, 0xFF, 100, 30), *v = pair(OP_ATTACK, 0xFF, 100, 30);
    VivreState Su, Sv; vivre_init(&Su, 77); vivre_init(&Sv, 77);
    for (long t = 0; t < 20; t++) { u->t = v->t = t; vivre_step(u, &all, &Su); vivre_step(v, &all, &Sv); }
    VCHECK("déterminisme des trois leviers", !memcmp(&Su, &Sv, sizeof Su)
           && !memcmp(u->cs[0], v->cs[0], sizeof(Creature)) && !memcmp(u->cs[1], v->cs[1], sizeof(Creature)));
    world_free(u); world_free(v);

    VivreSample s; w = pair(OP_ATTACK, OP_EAT, 100, 100); vivre_sample(w, &s);
    VCHECK("relevé : octets et fraction ATTACK", s.n == 2 && s.frac_attack == 0.5 && s.give_byte == (OP_ATTACK + OP_EAT) / 2.0);
    world_free(w);
    return fails;
}
#undef VCHECK
