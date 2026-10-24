#include "loi.h"
/* Lois locales : la table portée par chaque individu n'entre pas dans ce
 * contrat. Ce qui tient, c'est une marque réécrite à chaque relevé :
 * case (0,0) de chaque île, valeur 1. */
static int annonce;

static void echantillon(const LoiMonde *m) {
    if (!m || !m->poser_marque) return;
    if (!annonce && m->evenement) {
        annonce = 1;
        m->evenement("heritage", "marque 1 en (0,0) de chaque ile");
    }
    int K = m->K;
    if (K < 1) return;
    if (K > 32) K = 32;
    for (int ile = 0; ile < K; ile++) m->poser_marque(ile, 0, 0, 1);
}

const Loi *loi_registre(void) {
    static Loi L = { "heritage", 1, echantillon };
    return &L;
}
