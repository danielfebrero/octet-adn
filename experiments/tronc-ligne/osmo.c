#include "loi.h"
/* Osmotrophie de la soupe : une unité par île et par relevé, d'une case
 * qui en a plus de 2 vers sa voisine est, si celle-ci est sous 14.
 * La somme est conservée. Le génome copieur n'est pas rejoué. */
#define LW 64
#define LH 32
static int annonce;

static void echantillon(const LoiMonde *m) {
    if (!m || !m->lire_nourriture || !m->ajouter_nourriture) return;
    if (!annonce && m->evenement) {
        annonce = 1;
        m->evenement("osmo", "une unite par ile, d'une case riche vers l'est");
    }
    int K = m->K;
    if (K < 1) return;
    if (K > 32) K = 32;
    long t = m->tick < 0 ? 0 : m->tick;
    int off = (int)(t % (LW * LH));
    for (int ile = 0; ile < K; ile++) {
        for (int n = 0; n < LW * LH; n++) {
            int p = (off + n) % (LW * LH);
            int x = p % LW, y = p / LW;
            if (m->lire_nourriture(ile, x, y) <= 2) continue;
            int xe = (x + 1) % LW;
            if (m->lire_nourriture(ile, xe, y) >= 14) continue;
            m->ajouter_nourriture(ile, x, y, -1);
            m->ajouter_nourriture(ile, xe, y, 1);
            break;
        }
    }
}

const Loi *loi_registre(void) {
    static Loi L = { "osmo", 1, echantillon };
    return &L;
}
