#include "loi.h"
#include <stdio.h>
/* Marque 2 sur le coin haut-gauche de chaque carré 2×2 mixte
 * encore visible au relevé. La case (0,0) reste à l'héritage.
 * Ne retire rien. Ne pas déposer dans vie-tronc/lois/ :
 * ce processus n'a pas le champ lire_brique. */
#define LW 64
#define LH 32
static int vu;
static void echantillon(const LoiMonde *m) {
    if (!m || !m->lire_brique) return;
    int K = m->K; if (K < 0) K = 0; if (K > 32) K = 32;
    int mix = 0;
    for (int ile = 0; ile < K; ile++) {
        for (int y = 0; y < LH - 1; y++) for (int x = 0; x < LW - 1; x++) {
            int a = m->lire_brique(ile, x, y);
            int b = m->lire_brique(ile, x + 1, y);
            int c = m->lire_brique(ile, x, y + 1);
            int d = m->lire_brique(ile, x + 1, y + 1);
            if (!a || !b || !c || !d) continue;
            if (a == b && a == c && a == d) continue;
            mix++;
            if ((x || y) && m->poser_marque) m->poser_marque(ile, x, y, 2);
        }
    }
    if (mix > 0 && !vu && m->evenement) {
        char buf[80];
        snprintf(buf, sizeof buf, "carres=%d", mix);
        m->evenement("trace", buf);
    }
    vu = mix > 0;
}
const Loi *loi_registre(void) {
    static Loi L = { "trace", 1, echantillon };
    return &L;
}
