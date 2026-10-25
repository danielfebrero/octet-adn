/* Une seule liste pour le refus d'écriture et la garde de lecture. */
#ifndef OCTET_MONDES_COMPAT_H
#define OCTET_MONDES_COMPAT_H
#include <string.h>

#include "mondes.h"
#define octet_mondes_proteges octet_mondes
enum { OCTET_MONDES_PROTEGES = OCTET_NMONDES };

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
