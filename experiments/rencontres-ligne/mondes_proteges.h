/* Une seule liste pour le refus d'écriture et la garde de lecture. */
#ifndef OCTET_MONDES_PROTEGES_H
#define OCTET_MONDES_PROTEGES_H
#include <string.h>

static const char *const octet_mondes_proteges[] = {
    "vie-monde", "vie-forces", "vie-causes", "vie-forces-briques",
    "vie-forces-societes", "vie-attention", "vie-traits", "vie-depense",
    "vie-causales", "vie-saison", "vie-tronc", "vie-lecture",
    "vie-rencontres-b129054c", "vie-rencontres-sonnet-41ab85e"
};
enum { OCTET_MONDES_PROTEGES = sizeof octet_mondes_proteges / sizeof *octet_mondes_proteges };

static inline int octet_monde_protege(const char *path) {
    size_t n = strlen(path);
    while (n && path[n - 1] == '/') n--;
    size_t start = n;
    while (start && path[start - 1] != '/') start--;
    for (int i = 0; i < OCTET_MONDES_PROTEGES; i++)
        if (strlen(octet_mondes_proteges[i]) == n - start &&
            !memcmp(path + start, octet_mondes_proteges[i], n - start)) return i;
    return -1;
}
#endif
