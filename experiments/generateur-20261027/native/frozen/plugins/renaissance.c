#include "loi.h"
/* Filet de reprise. Au-dessus de 1000 individus, rien. En dessous, huit
 * unités sur l'île 0. La table d'opcodes de la recherche d'univers n'est
 * pas rejouée : ce contrat ne change pas les instructions. */
#define LW 64
#define LH 32
static int annonce;

static void echantillon(const LoiMonde *m) {
    if (!m) return;
    if (!annonce && m->evenement) {
        annonce = 1;
        m->evenement("renaissance", "en veille tant que la population tient");
    }
    if (m->pop >= 1000 || !m->ajouter_nourriture) return;
    long t = m->tick < 0 ? 0 : m->tick;
    int base = (int)(t % (LW * LH));
    for (int i = 0; i < 8; i++) {
        int p = (base + i * 17) % (LW * LH);
        m->ajouter_nourriture(0, p % LW, p / LW, 1);
    }
}

const Loi *loi_registre(void) {
    static Loi L = { "renaissance", 1, echantillon };
    return &L;
}

/* Generated adapter checkpoint extension; no change to the physical law. */
void adapter_state_get(int*p){p[0]=annonce;}
void adapter_state_set(const int*p){annonce=p[0];}
