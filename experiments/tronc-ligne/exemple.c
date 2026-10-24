#include "loi.h"
/* Exemple inerte : il s'annonce une fois. Le déposer dans vie-tronc/lois/
 * le branche au relevé suivant. Tant qu'il n'y est pas, il ne tourne pas. */
static int annonce;
static void echantillon(const LoiMonde *m) {
    if (annonce || !m || !m->evenement) return;
    annonce = 1;
    m->evenement("loi", "exemple branche");
}
const Loi *loi_registre(void) {
    static Loi L = { "exemple", 1, echantillon };
    return &L;
}
