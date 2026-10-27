/* Branche d'un monde tronc. Un .so déposé dans <dir>/lois/ est chargé
 * au relevé suivant, sans relinker le binaire qui tourne.
 * loi_registre() renvoie cette structure. version = 1.
 * Le pointeur LoiMonde n'est valable que le temps de echantillon.
 */
#ifndef OCTET_LOI_H
#define OCTET_LOI_H
typedef struct LoiMonde {
    long tick;
    int pop;
    int K;
    void (*evenement)(const char *type, const char *detail);
    int (*lire_nourriture)(int ile, int x, int y);
    void (*ajouter_nourriture)(int ile, int x, int y, int delta);
    void (*retirer_brique)(int ile, int x, int y);
    void (*ajouter_chaleur)(int ile, int x, int y, int delta);
    void (*poser_marque)(int ile, int x, int y, int valeur);
    /* En queue, pour que les .so déjà compilés gardent leurs offsets.
     * NULL chez un hôte ancien : ne pas déposer un .so qui le lit
     * dans le dossier lois/ de ce processus. */
    int (*lire_brique)(int ile, int x, int y);
} LoiMonde;
typedef struct Loi {
    const char *nom;
    int version;
    void (*echantillon)(const LoiMonde *monde);
} Loi;
const Loi *loi_registre(void);
#endif
