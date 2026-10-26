#define main relief_original_main
#include "../relief-20261025/relief.c"
#undef main
/* Ombre de Relief : rejoue le monde tick par tick, en mémoire, à partir de son dernier checkpoint.
 *
 * Le moteur est déterministe (état complet + RNG dans le checkpoint) et step() n'écrit rien quand
 * w.events == NULL ; l'échantillonnage des événements ne fait pas avancer le RNG. L'ombre calcule donc
 * exactement les mêmes ticks que le monde vivant. Elle ne l'interrompt pas, ne le lit qu'en lecture seule
 * (state.ck, series.tsv) et n'écrit que sur sa sortie standard.
 *
 * À chaque nouveau checkpoint du vivant, l'ombre compare son propre état au même tick (tick, RNG,
 * naissances, décès, population). Égal : c'est la preuve que les mouvements montrés sont les vrais.
 * Différent : elle recharge le checkpoint et le dit.
 *
 * Trames sur stdout : "RLV1", type (K image complète, D mouvements, V vérification), 3 octets nuls,
 * longueur u32, tick u64, puis la charge utile. Petit-boutiste. */
#include <time.h>

enum { PACKET = 5, KEY = 50, RING = 8192 };
typedef struct { uint64_t tick, rng, births, deaths; uint32_t pop; } Mark;
static Mark ring[RING];
static uint32_t *prev_id; static uint16_t *prev_x, *prev_y;
static uint8_t *buf; static size_t cap_buf;
static uint64_t last_emit;   /* tick de la dernière trame K ou D : base des mouvements suivants */

static void put16(uint8_t **p, uint32_t v) { (*p)[0] = (uint8_t)v; (*p)[1] = (uint8_t)(v >> 8); *p += 2; }
static void put32(uint8_t **p, uint32_t v) { for (int i = 0; i < 4; i++) (*p)[i] = (uint8_t)(v >> (8 * i)); *p += 4; }
static void put64(uint8_t **p, uint64_t v) { for (int i = 0; i < 8; i++) (*p)[i] = (uint8_t)(v >> (8 * i)); *p += 8; }
static void frame(char type, uint64_t tick, const uint8_t *payload, size_t n) {
    uint8_t h[20], *p = h; memcpy(p, "RLV1", 4); p += 4; *p++ = (uint8_t)type; *p++ = 0; *p++ = 0; *p++ = 0;
    put32(&p, (uint32_t)n); put64(&p, tick);
    if (fwrite(h, 1, 20, stdout) != 20 || (n && fwrite(payload, 1, n, stdout) != n) || fflush(stdout)) exit(0);   /* lecteur parti */
}
static uint8_t *room(size_t n) { if (n > cap_buf) { free(buf); buf = malloc(n); if (!buf) fail("ombre: mémoire"); cap_buf = n; } return buf; }
static uint16_t clamp16(uint64_t v) { return v > 65535 ? 65535 : (uint16_t)v; }

/* Image complète : par case d'agent, id (32 bits bas), x, y, lignée, énergie ; id 0 = case vide. */
static void key(const World *w) {
    uint32_t c = w->c.capacity; uint8_t *p = room(4 + (size_t)c * 12), *s = p; put32(&p, c);
    for (uint32_t i = 0; i < c; i++) {
        const Agent *a = &w->agents[i];
        uint32_t id = a->alive ? (uint32_t)a->id : 0; if (a->alive && !id) id = 1;
        put32(&p, id); put16(&p, a->pos % w->c.width); put16(&p, a->pos / w->c.width);
        put16(&p, clamp16(a->lineage)); put16(&p, clamp16(a->energy));
        prev_id[i] = id; prev_x[i] = (uint16_t)(a->pos % w->c.width); prev_y[i] = (uint16_t)(a->pos / w->c.width);
    }
    frame('K', w->tick, s, (size_t)(p - s)); last_emit = w->tick;
}
/* Mouvements depuis la trame précédente : morts, naissances, grands sauts, puis un octet par case
 * (dx+8)<<4 | (dy+8), 0x88 = immobile ou sans objet. */
static void delta(const World *w, uint64_t from) {
    uint32_t c = w->c.capacity, nd = 0, nb = 0, nj = 0;
    for (uint32_t i = 0; i < c; i++) {
        const Agent *a = &w->agents[i]; uint32_t id = a->alive ? ((uint32_t)a->id ? (uint32_t)a->id : 1) : 0;
        if (prev_id[i] && prev_id[i] != id) nd++;
        if (id && id != prev_id[i]) nb++;
        if (id && id == prev_id[i]) { int dx = (int)(a->pos % w->c.width) - prev_x[i], dy = (int)(a->pos / w->c.width) - prev_y[i];
            if (dx < -8 || dx > 7 || dy < -8 || dy > 7) nj++; }
    }
    uint8_t *p = room(8 + 12 + (size_t)nd * 2 + (size_t)nb * 12 + (size_t)nj * 6 + c), *s = p;
    put64(&p, from); put32(&p, nd); put32(&p, nb); put32(&p, nj);
    uint8_t *pd = p, *pb = pd + (size_t)nd * 2, *pj = pb + (size_t)nb * 12, *pm = pj + (size_t)nj * 6;
    for (uint32_t i = 0; i < c; i++) {
        const Agent *a = &w->agents[i]; uint32_t id = a->alive ? ((uint32_t)a->id ? (uint32_t)a->id : 1) : 0;
        uint16_t x = (uint16_t)(a->pos % w->c.width), y = (uint16_t)(a->pos / w->c.width); uint8_t m = 0x88;
        if (prev_id[i] && prev_id[i] != id) put16(&pd, i);
        if (id && id != prev_id[i]) { put16(&pb, i); put16(&pb, x); put16(&pb, y); put16(&pb, clamp16(a->lineage)); put32(&pb, id); }
        if (id && id == prev_id[i]) {
            int dx = (int)x - prev_x[i], dy = (int)y - prev_y[i];
            if (dx < -8 || dx > 7 || dy < -8 || dy > 7) { put16(&pj, i); put16(&pj, x); put16(&pj, y); }
            else m = (uint8_t)(((dx + 8) << 4) | (dy + 8));
        }
        pm[i] = m; prev_id[i] = id; prev_x[i] = x; prev_y[i] = y;
    }
    frame('D', w->tick, s, (size_t)(pm + c - s)); last_emit = w->tick;
}
static void remember(const World *w) { Mark *m = &ring[w->tick % RING]; m->tick = w->tick; m->rng = w->rng; m->births = w->births; m->deaths = w->deaths; m->pop = w->population; }

/* Lecture seule de l'en-tête du checkpoint vivant (tick, RNG, compteurs) ; ignore une lecture concurrente ratée. */
static int peek(const char *dir, Header *h, struct timespec *mtime) {
    char p[4096]; path(p, sizeof p, dir, "state.ck"); struct stat st; if (stat(p, &st)) return 0;
    FILE *f = fopen(p, "rb"); if (!f) return 0; int ok = fread(h, sizeof *h, 1, f) == 1; fclose(f);
    *mtime = st.st_mtim; return ok && h->magic == MAGIC && h->version == VERSION;
}
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_REALTIME, &t); return t.tv_sec + t.tv_nsec / 1e9; }
/* Tick du vivant à cet instant : dernier relevé de series.tsv (tous les 100 ticks) + temps écoulé. */
static uint64_t estimate(const char *dir, uint64_t fallback_tick, double fallback_time, unsigned tick_ms) {
    char p[4096]; path(p, sizeof p, dir, "series.tsv"); struct stat st; uint64_t t = fallback_tick; double at = fallback_time;
    FILE *f = fopen(p, "rb");
    if (f && !fstat(fileno(f), &st) && st.st_size > 0) {
        char tail[2048]; long n = st.st_size < (long)sizeof tail - 1 ? (long)st.st_size : (long)sizeof tail - 1;
        if (!fseek(f, -n, SEEK_END) && fread(tail, 1, (size_t)n, f) == (size_t)n) {
            tail[n] = 0; while (n > 0 && (tail[n - 1] == '\n' || tail[n - 1] == '\r')) tail[--n] = 0;
            char *line = strrchr(tail, '\n'); line = line ? line + 1 : tail;
            if (line[0] >= '0' && line[0] <= '9') {
                uint64_t v = strtoull(line, NULL, 10);
                double m = st.st_mtim.tv_sec + st.st_mtim.tv_nsec / 1e9;
                if (v >= t) { t = v; at = m; }
            }
        }
    }
    if (f) fclose(f);
    double ahead = (now_s() - at) * 1000.0 / tick_ms; if (ahead < 0) ahead = 0; if (ahead > 120) ahead = 120;   /* vivant en reprise : on n'invente pas */
    return t + (uint64_t)ahead;
}
static void sleep_ms(unsigned ms) { struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L }; while (nanosleep(&t, &t) && errno == EINTR) {} }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: ombre MONDE [tick_ms]\n"); return 2; }
    const char *dir = argv[1]; unsigned tick_ms = argc > 2 ? number32(argv[2]) : 100; if (!tick_ms || tick_ms > 1000) tick_ms = 100;
    World w = load(dir); w.events = NULL;
    Header h; struct timespec seen; if (!peek(dir, &h, &seen)) fail("ombre: en-tête");
    double loaded_at = seen.tv_sec + seen.tv_nsec / 1e9;
    uint32_t c = w.c.capacity;
    prev_id = calloc(c, 4); prev_x = calloc(c, 2); prev_y = calloc(c, 2); if (!prev_id || !prev_x || !prev_y) fail("ombre: mémoire");
    remember(&w); key(&w);
    uint64_t base = w.tick, last_check = 0; double last_poll = 0;
    for (;;) {
        double t0 = now_s();
        /* nouveau checkpoint du vivant ? on vérifie l'ombre au même tick */
        if (t0 - last_poll > 1.0) {
            last_poll = t0; Header nh; struct timespec m;
            if (peek(dir, &nh, &m) && (m.tv_sec != seen.tv_sec || m.tv_nsec != seen.tv_nsec) && nh.state.tick != last_check) {
                seen = m; uint64_t ct = nh.state.tick;
                while (w.population && w.tick < ct) { step(&w); remember(&w); if (w.tick % PACKET == 0) { delta(&w, last_emit); if (w.tick % KEY == 0) key(&w); } }
                Mark *k = &ring[ct % RING];
                int ok = k->tick == ct && k->rng == nh.state.rng && k->births == nh.state.births && k->deaths == nh.state.deaths && k->pop == nh.state.population;
                uint8_t v[17], *p = v; put64(&p, ct); put64(&p, nh.state.rng); *p++ = (uint8_t)ok;
                frame('V', w.tick, v, 17); last_check = ct;
                if (!ok) {   /* désaccord : on se recale sur le vivant, sans rien deviner */
                    release(&w); w = load(dir); w.events = NULL; remember(&w); key(&w);
                    loaded_at = m.tv_sec + m.tv_nsec / 1e9; base = w.tick;
                }
            }
        }
        if (!w.population) { sleep_ms(1000); continue; }
        uint64_t target = estimate(dir, base, loaded_at, tick_ms);
        if (w.tick + 15 < target) {                       /* rattrapage, sans pause */
            step(&w); remember(&w);
        } else if (w.tick > target + 5) {                 /* en avance : on attend le vivant */
            sleep_ms(tick_ms); continue;
        } else {                                          /* au rythme du vivant */
            step(&w); remember(&w);
            double spent = (now_s() - t0) * 1000.0; if (spent < tick_ms) sleep_ms(tick_ms - (unsigned)spent);
        }
        if (w.tick % PACKET == 0) { delta(&w, last_emit); if (w.tick % KEY == 0) key(&w); }
    }
}
