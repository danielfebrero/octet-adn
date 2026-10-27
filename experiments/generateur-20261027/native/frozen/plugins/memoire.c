#include "loi.h"
/* Mémoire du relevé précédent : une chaleur sur une case de l'île 0 si la
 * population a baissé. L'événement ne part qu'à la première baisse et se
 * réarme quand la population remonte. Les traces de mémoire-2 ne sont pas
 * rejouées. */
#define LW 64
#define LH 32
static int annonce, prev, dit;

static void echantillon(const LoiMonde *m) {
    if (!m) return;
    if (!annonce) {
        annonce = 1;
        prev = m->pop;
        if (m->evenement) m->evenement("memoire", "armee, une chaleur si la population baisse");
        return;
    }
    if (m->pop < prev) {
        if (m->ajouter_chaleur) {
            long t = m->tick < 0 ? 0 : m->tick;
            m->ajouter_chaleur(0, (int)((t / LH) % LW), (int)(t % LH), 1);
        }
        if (!dit && m->evenement) {
            dit = 1;
            m->evenement("memoire", "population en baisse, une chaleur");
        }
    } else if (m->pop > prev) {
        dit = 0;
    }
    prev = m->pop;
}

const Loi *loi_registre(void) {
    static Loi L = { "memoire", 1, echantillon };
    return &L;
}

/* Generated adapter checkpoint extension; no change to the physical law. */
void adapter_state_get(int*p){p[0]=annonce;p[1]=prev;p[2]=dit;}
void adapter_state_set(const int*p){annonce=p[0];prev=p[1];dit=p[2];}
