/* Persistance 100 % binaire : checkpoints (.ck), métriques (.bin), champions (.adn bruts).
 * Seul status.md est du texte, pour l'humain. Format little-endian (x86_64). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "octet.h"

static int put_entry(FILE *f, const PoolEntry *e) {
    uint8_t sp = (uint8_t)e->species; uint16_t n = (uint16_t)e->len;
    return fwrite(&sp, 1, 1, f) == 1 && fwrite(&n, 2, 1, f) == 1 && fwrite(e->dna, 1, n, f) == n;
}
static int get_entry(FILE *f, PoolEntry *e) {
    uint8_t sp; uint16_t n;
    if (fread(&sp, 1, 1, f) != 1 || fread(&n, 2, 1, f) != 1 || n > MAX_LEN) return 0;
    e->species = sp; e->len = n;
    return fread(e->dna, 1, n, f) == n;
}

/* OADNCKP1 | u8 lab | u32 cycle | f64 best | u64 engine | u8 has_best | [entry] | u16 npool | entry* */
int ck_save(const char *path, const Checkpoint *ck) {
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    uint8_t lab = (uint8_t)ck->lab_id, hb = (uint8_t)ck->has_best; uint16_t np = (uint16_t)ck->npool;
    int ok = fwrite(CK_MAGIC, 8, 1, f) == 1 && fwrite(&lab, 1, 1, f) == 1 && fwrite(&ck->cycle, 4, 1, f) == 1
          && fwrite(&ck->best_fitness, 8, 1, f) == 1 && fwrite(&ck->engine, 8, 1, f) == 1 && fwrite(&hb, 1, 1, f) == 1;
    if (ok && hb) ok = put_entry(f, &ck->best);
    ok = ok && fwrite(&np, 2, 1, f) == 1;
    for (int i = 0; ok && i < ck->npool; i++) ok = put_entry(f, &ck->pool[i]);
    ok = (fclose(f) == 0) && ok;
    if (!ok || rename(tmp, path)) { remove(tmp); return -1; }
    return 0;
}
int ck_load(const char *path, Checkpoint *ck) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char mg[8]; uint8_t lab, hb; uint16_t np;
    memset(ck, 0, sizeof *ck);
    int ok = fread(mg, 8, 1, f) == 1 && !memcmp(mg, CK_MAGIC, 8) && fread(&lab, 1, 1, f) == 1
          && fread(&ck->cycle, 4, 1, f) == 1 && fread(&ck->best_fitness, 8, 1, f) == 1
          && fread(&ck->engine, 8, 1, f) == 1 && fread(&hb, 1, 1, f) == 1;
    ck->lab_id = lab; ck->has_best = hb;
    if (ok && hb) ok = get_entry(f, &ck->best);
    ok = ok && fread(&np, 2, 1, f) == 1 && np <= POOL_MAX;
    ck->npool = ok ? np : 0;
    for (int i = 0; ok && i < ck->npool; i++) ok = get_entry(f, &ck->pool[i]);
    fclose(f);
    return ok ? 0 : -1;
}

int met_append(const char *path, const MetricRec *r) {
    FILE *f = fopen(path, "ab");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0) fwrite(MET_MAGIC, 8, 1, f);
    int ok = fwrite(r, sizeof *r, 1, f) == 1;
    return (fclose(f) == 0 && ok) ? 0 : -1;
}
int met_read(const char *path, MetricRec **out) {
    *out = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char mg[8];
    if (fread(mg, 8, 1, f) != 1 || memcmp(mg, MET_MAGIC, 8)) { fclose(f); return -1; }
    int n = 0, cap = 64;
    MetricRec *v = xmalloc(sizeof *v * (size_t)cap);
    while (fread(&v[n], sizeof *v, 1, f) == 1)
        if (++n == cap) v = xrealloc(v, sizeof *v * (size_t)(cap *= 2));
    fclose(f);
    *out = v;
    return n;
}

static void iso_ts(int64_t ts, int32_t off, char *out, size_t cap) {
    time_t t = (time_t)(ts + off);
    struct tm tm; gmtime_r(&t, &tm);
    char b[32]; strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%S", &tm);
    int a = off < 0 ? -off : off;
    snprintf(out, cap, "%s%c%02d:%02d", b, off < 0 ? '-' : '+', a / 3600, (a % 3600) / 60);
}

static size_t fmt_extra(const MetricRec *r, char *out, size_t cap) {
    size_t o = 0;
    if (r->lab_id >= N_LABS) return 0;
    for (int i = 0; i < N_EXTRA && LABS[r->lab_id].extra_names[i]; i++)
        o += (size_t)snprintf(out + o, cap - o, "%s%s=%.6g", i ? ", " : "", LABS[r->lab_id].extra_names[i], r->extra[i]);
    return o;
}

void met_format(const MetricRec *r, char *out, size_t cap) {
    char ts[40], ex[512];
    iso_ts(r->ts, r->tz_off, ts, sizeof ts);
    fmt_extra(r, ex, sizeof ex);
    snprintf(out, cap,
             "ts=%s lab=%s cycle=%u ticks=%u survived=%u fitness=%.5g best=%.5g accepted=%u "
             "pop_final=%u pop_mean=%.2f born=%u dead=%u store_writes=%u useful_stores=%u emits=%u "
             "genome_len_mean=%.1f engine=%010llx state_hash=%016llx elapsed_s=%.3f extra={%s}",
             ts, r->lab_id < N_LABS ? LABS[r->lab_id].name : "?", r->cycle, r->ticks, r->survived,
             r->fitness, r->best_fitness, r->accepted, r->pop_final, r->pop_mean, r->born, r->dead,
             r->store_writes, r->useful_stores, r->emits, r->genome_len_mean,
             (unsigned long long)(r->engine & 0xffffffffffULL), (unsigned long long)r->state_hash, r->elapsed_s, ex);
}

static const char *BARS[8] = { "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█" };

int write_status(const char *dir, const int *labs, int nlabs) {
    char path[4096];
    snprintf(path, sizeof path, "%s/status.md", dir);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# Octet-ADN — état des éprouvettes\n\n"
               "| labo | cycle | fitness | meilleure | pop fin | génome moy. (o) | moteur | tendance (30 derniers) | détails |\n"
               "|---|---:|---:|---:|---:|---:|---|---|---|\n");
    int64_t last = 0; int32_t off = 0;
    for (int k = 0; k < nlabs; k++) {
        const char *name = LABS[labs[k]].name;
        char mp[4096]; MetricRec *v;
        snprintf(mp, sizeof mp, "%s/%s/metrics.bin", dir, name);
        int n = met_read(mp, &v);
        if (n <= 0) { fprintf(f, "| %s | – | – | – | – | – | – | – | pas encore lancé |\n", name); free(v); continue; }
        MetricRec *r = &v[n - 1];
        if (r->ts > last) { last = r->ts; off = r->tz_off; }
        int s0 = n > 30 ? n - 30 : 0; double lo = 1e300, hi = -1e300;
        for (int i = s0; i < n; i++) { if (v[i].fitness < lo) lo = v[i].fitness; if (v[i].fitness > hi) hi = v[i].fitness; }
        double span = hi - lo > 0 ? hi - lo : 1.0;
        char sp[256] = ""; size_t o = 0;
        for (int i = s0; i < n; i++) {
            int b = (int)((v[i].fitness - lo) / span * 7.999); if (b > 7) b = 7; if (b < 0) b = 0;
            o += (size_t)snprintf(sp + o, sizeof sp - o, "%s", BARS[b]);
        }
        char ex[512]; fmt_extra(r, ex, sizeof ex);
        fprintf(f, "| %s | %u | %.4g | %.4g | %u | %.0f | `%010llx` | %s | %s |\n", name, r->cycle, r->fitness,
                r->best_fitness, r->pop_final, r->genome_len_mean, (unsigned long long)(r->engine & 0xffffffffffULL), sp, ex);
        free(v);
    }
    char ts[40] = "–";
    if (last) iso_ts(last, off, ts, sizeof ts);
    fprintf(f, "\nDernière mise à jour : %s\n\n"
               "`moteur` = empreinte du binaire `octet-adn` : quand il est recompilé, le runner se ré-exécute "
               "et l'amélioration de la VM/ISA (la « popote ») s'applique à tous les labos dès le cycle suivant.\n", ts);
    return fclose(f);
}
