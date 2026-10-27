#include "loi.h"
#include <stdio.h>
/* Lecture seule. Compte les briques une fois et se tait.
 * Ne retire rien. Ne pas déposer dans vie-tronc/lois/ :
 * ce processus n'a pas le champ lire_brique. */
#define LW 64
#define LH 32
static int annonce;
static void echantillon(const LoiMonde *m) {
    if (!m || !m->lire_brique || annonce) return;
    annonce = 1;
    int K = m->K; if (K < 0) K = 0; if (K > 32) K = 32;
    long n = 0; int mix = 0;
    for (int ile = 0; ile < K; ile++) {
        for (int y = 0; y < LH; y++) for (int x = 0; x < LW; x++)
            if (m->lire_brique(ile, x, y)) n++;
        for (int y = 0; y < LH - 1; y++) for (int x = 0; x < LW - 1; x++) {
            int a = m->lire_brique(ile, x, y), b = m->lire_brique(ile, x + 1, y);
            int c = m->lire_brique(ile, x, y + 1), d = m->lire_brique(ile, x + 1, y + 1);
            if (a && b && c && d && !(a == b && a == c && a == d)) mix++;
        }
    }
    if (m->evenement) {
        char buf[120];
        snprintf(buf, sizeof buf, "briques=%ld carres_mixtes=%d", n, mix);
        m->evenement("lecture", buf);
    }
}
const Loi *loi_registre(void) {
    static Loi L = { "lecture", 1, echantillon };
    return &L;
}

/* Generated adapter checkpoint extension; no change to the physical law. */
void adapter_state_get(int*p){p[0]=annonce;}
void adapter_state_set(const int*p){annonce=p[0];}
