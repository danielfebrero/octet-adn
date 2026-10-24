#include "loi.h"
/* Compétition entre îles : une unité par relevé, de l'île la plus nourrie
 * vers la plus pauvre. Scan déterministe, aucun tirage. Les génomes de
 * compétition-ligne ne sont pas rejoués. */
#define LW 64
#define LH 32
static int annonce;

static int somme(const LoiMonde *m, int ile) {
    int s = 0;
    for (int y = 0; y < LH; y++)
        for (int x = 0; x < LW; x++)
            s += m->lire_nourriture(ile, x, y);
    return s;
}

/* riche : première case > 0. pauvre : première case < 14. -1 si aucune. */
static int trouve(const LoiMonde *m, int ile, int off, int riche) {
    for (int n = 0; n < LW * LH; n++) {
        int p = (off + n) % (LW * LH);
        int f = m->lire_nourriture(ile, p % LW, p / LW);
        if (riche ? f > 0 : f < 14) return p;
    }
    return -1;
}

static void echantillon(const LoiMonde *m) {
    if (!m || !m->lire_nourriture || !m->ajouter_nourriture) return;
    if (!annonce && m->evenement) {
        annonce = 1;
        m->evenement("competition", "une unite, de l'ile la plus riche vers la plus pauvre");
    }
    int K = m->K;
    if (K < 2) return;
    if (K > 32) K = 32;
    int best = 0, worst = 0;
    int sb = somme(m, 0), sw = sb;
    for (int ile = 1; ile < K; ile++) {
        int s = somme(m, ile);
        if (s > sb) { sb = s; best = ile; }
        if (s < sw) { sw = s; worst = ile; }
    }
    if (best == worst || sb <= sw) return;
    long t = m->tick < 0 ? 0 : m->tick;
    int off = (int)(t % (LW * LH));
    int pr = trouve(m, best, off, 1);
    int pp = trouve(m, worst, off, 0);
    if (pr < 0 || pp < 0) return;
    m->ajouter_nourriture(best, pr % LW, pr / LW, -1);
    m->ajouter_nourriture(worst, pp % LW, pp / LW, 1);
}

const Loi *loi_registre(void) {
    static Loi L = { "competition", 1, echantillon };
    return &L;
}
