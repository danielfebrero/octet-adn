/* Une seule liste pour le refus d'écriture et l'inventaire de garde.
 * Les fixtures restent un contrôle de code, jamais une preuve de l'hôte. */
#ifndef OCTET_MONDES_PROTEGES_H
#define OCTET_MONDES_PROTEGES_H
static const char *const octet_mondes[] = {
    "vie-monde", "vie-forces", "vie-causes", "vie-forces-briques",
    "vie-forces-societes", "vie-attention", "vie-traits", "vie-depense",
    "vie-causales", "vie-saison", "vie-tronc", "vie-lecture",
    "vie-rencontres-b129054c", "vie-rencontres-sonnet-41ab85e",
    "vie-rencontres-claude-3a1923e", "vie-capture-3a1923e",
    "vie-parente-3a1923e", "vie-senescence-3a1923e"
};
enum { OCTET_NMONDES = sizeof octet_mondes / sizeof octet_mondes[0] };
#endif
