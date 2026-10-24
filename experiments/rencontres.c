/* ELF neuf : trois physiques désactivables, aucun changement du cœur src/.
 * Les extensions sont sauvegardées dans le même tmp+rename que le terrarium.
 * Ne jamais compiler ce fichier vers le chemin d'un ELF déjà en marche. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sys/file.h>
#include <fcntl.h>
#include "octet.h"
#include "rencontres-ligne/echange.h"
#include "rencontres-ligne/naissance.h"
#include "rencontres-ligne/rendezvous.h"

static void extra_before(void *);
static void extra_after(void *);
static void extra_hook(void *, void *);
static void extra_sample(void *);
static int extra_save(FILE *, void *);
static int extra_load(FILE *, void *);
#define TERRA_BEFORE_WORLD(I) extra_before(I)
#define TERRA_AFTER_WORLD(I) extra_after(I)
#define TERRA_AFTER_HOOK(I, c) extra_hook(I, c)
#define TERRA_AFTER_SAMPLE(T) extra_sample(T)
#define TERRA_SAVE_EXTRA(f, T) extra_save(f, T)
#define TERRA_LOAD_EXTRA(f, T) extra_load(f, T)
#define main terrarium_original_main
#include "terrarium.c"
#undef main

enum { CONTROL, EXCHANGE, NURSERY, MEETING, ALL, NPROFILE };
static const char *profiles[] = { "control", "exchange", "nursery", "meeting", "all" };
typedef struct {
    int profile, reserve;
    EchangeConfig exchange;
    NaissanceConfig birth;
    RendezvousConfig meeting;
} Config;
typedef struct {
    int ready;
    EchangeState exchange;
    NaissanceState birth;
    RendezvousState meeting;
    long contacts, intra, inter, exchange_at_cap, birth_no_death;
    long before_dead, before_hatched;
} ExtraIsland;
static Config config;
static ExtraIsland ext[KMAX];
static FILE *metrics;
static unsigned announced;
static int profile_explicit, loading, extension_error, bypass;

static Config configuration(int profile) {
    Config c = {0};
    c.profile = profile; c.reserve = 4; /* quart du cap, pas quatre individus */
    /* Choix primaire conteste : cooldown=64 (mesure b129054c) refusait 56,8% des contacts
     * geometriques pour la seule raison du repos (exchange_cooldown_refused / contacts,
     * moyenne sur 8 graines, 4000 ticks, profil exchange), contre 0% a cooldown=0. Comme
     * period=16, tout cooldown <= 16 est un no-op strict (tick - dernier < cooldown n'est
     * jamais vrai au passage suivant) : 32 est la PLUS PETITE valeur qui protege reellement
     * une paire contre sa propre reconduction immediate. A 32, le blocage mesure tombe a
     * 42,3% (voir experiments/rencontres-ligne/RESULTATS.md) : la protection contre le
     * couple fixe qui s'echange a chaque passage reste entiere, sans doubler le frein.
     */
    c.exchange = (EchangeConfig){ .enabled = profile == EXCHANGE || profile == ALL,
        .period = 16, .cost = 3, .cooldown = 32 };
    c.birth = (NaissanceConfig){ .enabled = profile == NURSERY || profile == ALL,
        .period = 16, .cost = 24, .food_cost = 2, .delay = 24,
        .season_period = 400, .season_open = 160, .mutation = 0.005 };
    c.meeting = (RendezvousConfig){ .enabled = profile == MEETING || profile == ALL,
        .period = 4, .cost = 1 };
    return c;
}

static ExtraIsland *extra_island(Island *I) {
    ExtraIsland *e = &ext[I->idx];
    if (!e->ready) {
        uint64_t seed = I->P->seed * 1000003ULL + (uint64_t)I->idx * 104729;
        echange_init(&e->exchange, seed ^ 0xECA6E);
        naissance_init(&e->birth, seed ^ 0x0E0F);
        rendezvous_init(&e->meeting, seed ^ 0xAEDE);
        e->ready = 1;
    }
    return e;
}

/* Observations de paires physiques, une fois par passage et par paire i<j.
 * Une même paire peut revenir au passage suivant. Ce ne sont pas des sociétés. */
static void contacts(Island *I, ExtraIsland *e) {
    World *w = I->w;
    if (w->t % 16) return;
    for (int i = 0; i < w->ncs; i++) for (int j = i + 1; j < w->ncs; j++) {
        Creature *a = w->cs[i], *b = w->cs[j];
        if (a->e <= 0 || b->e <= 0) continue;
        int dx = abs(a->x - b->x), dy = abs(a->y - b->y);
        if (dx > w->cfg.w - dx) dx = w->cfg.w - dx;
        if (dy > w->cfg.h - dy) dy = w->cfg.h - dy;
        if (dx > 1 || dy > 1) continue;
        e->contacts++;
        if (a->species == b->species) e->intra++; else e->inter++;
    }
}

static void extra_before(void *ptr) {
    if (bypass) return;
    Island *I = ptr; World *w = I->w;
    ExtraIsland *e = extra_island(I);
    contacts(I, e);
    long before = e->exchange.changes;
    echange_step(w, &config.exchange, &e->exchange);
    if (e->exchange.changes != before) {
        if (w->ncs >= I->P->max_pop) e->exchange_at_cap += e->exchange.changes - before;
        for (int i = 0; i < w->ncs; i++) genotype(w->cs[i]);
    }
    e->before_dead = w->st.dead; e->before_hatched = e->birth.hatched;
    if (config.birth.enabled) {
        int reserve = I->P->max_pop / config.reserve;
        w->cfg.max_pop = I->P->max_pop - reserve;
    }
}

static void extra_hook(void *ptr, void *creature) {
    if (bypass) return;
    Island *I = ptr;
    rendezvous_before(I->w, creature, &config.meeting, &extra_island(I)->meeting, I->mark);
}

static void extra_after(void *ptr) {
    if (bypass) return;
    Island *I = ptr; World *w = I->w;
    ExtraIsland *e = extra_island(I);
    if (config.birth.enabled) w->cfg.max_pop = I->P->max_pop;
    NaissanceConfig b = config.birth; b.cap = I->P->max_pop;
    naissance_step(w, &b, &e->birth);
    if (w->st.dead == e->before_dead) e->birth_no_death += e->birth.hatched - e->before_hatched;
    rendezvous_fields(w, &config.meeting, &e->meeting, NULL);
}

/* exchange_cooldown_refused/partner_refused/naissance_cooldown_refused/food_refused/
 * queue_refused etaient deja comptes dans EchangeState/NaissanceState mais jamais ecrits
 * ici ; RESULTATS.md le signalait comme limite ("ils ne sont pas estimes ici"). Sans eux,
 * impossible de distinguer "peu d'echanges parce que peu de contacts" de "peu d'echanges
 * parce que le repos bloque" : question posee par le choix primaire cooldown=64. */
static const char *METRICS = "tick\tcontacts\tintra_contacts\tinter_contacts\texchanges\texchange_at_cap\tbytes_changed\teggs\thatched\tcrossed_children\tbirth_no_death\tcap_refused\tplace_refused\tenergy_refused\tseason_refused\treads\tmoves\tmove_energy\tpending\texchange_cooldown_refused\tpartner_refused\tnaissance_cooldown_refused\tfood_refused\tqueue_refused\n";
static void extra_sample(void *ptr) {
    Terra *T = ptr;
    long ct = 0, in = 0, out = 0, xc = 0, full = 0, bytes = 0, eggs = 0, born = 0, cross = 0, nd = 0;
    long cap = 0, place = 0, energy = 0, season = 0, reads = 0, moves = 0, paid = 0, pending = 0;
    long xcool = 0, partner = 0, bcool = 0, food = 0, queue = 0;
    for (int k = 0; k < T->P.K; k++) {
        ExtraIsland *e = extra_island(&T->I[k]);
        ct += e->contacts; in += e->intra; out += e->inter;
        xc += e->exchange.changes; full += e->exchange_at_cap; bytes += e->exchange.bytes_changed;
        eggs += e->birth.eggs; born += e->birth.hatched; cross += e->birth.crossed; nd += e->birth_no_death;
        cap += e->birth.refused_cap; place += e->birth.refused_place;
        energy += e->birth.refused_energy + e->exchange.energy_refused;
        season += e->birth.refused_season;
        reads += e->meeting.reads; moves += e->meeting.moves; paid += e->meeting.paid;
        pending += e->birth.npending;
        xcool += e->exchange.cooldown_refused;
        partner += e->birth.refused_partner; bcool += e->birth.refused_cooldown;
        food += e->birth.refused_food; queue += e->birth.refused_queue;
    }
    if (!metrics && !extension_error) {
        char path[600]; snprintf(path, sizeof path, "%s/rencontres.tsv", T->dir);
        int fresh = access(path, F_OK) != 0;
        if (loading) truncate_tsv(path, T->t - T->P.sample);
        metrics = fopen(path, loading ? "a" : "w");
        if (!metrics) { extension_error = 1; stop_flag = 1; return; }
        if (!loading || fresh) fputs(METRICS, metrics);
    }
    if (metrics) {
        fprintf(metrics, "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
            T->t, ct, in, out, xc, full, bytes, eggs, born, cross, nd, cap, place, energy, season, reads, moves, paid, pending,
            xcool, partner, bcool, food, queue);
        if (fflush(metrics)) { extension_error = 1; stop_flag = 1; }
    }
    if (xc && !(announced & 1)) { event(T, "croisement_rencontre", "couples_modifies=%ld au_plafond=%ld", xc, full); announced |= 1; }
    if (born && !(announced & 2)) { event(T, "eclosion", "enfants=%ld croises=%ld sans_mort_ce_tick=%ld", born, cross, nd); announced |= 2; }
    if (moves && !(announced & 4)) { event(T, "rendezvous", "pas_coutes=%ld lectures=%ld", moves, reads); announced |= 4; }
}

/* Version et tailles explicites : pas de changement de Params ou de LoiMonde.
 * Une seule publication atomique contient monde, œufs, cooldowns, marques et RNG. */
static int extra_save(FILE *f, void *ptr) {
    Terra *T = ptr; uint32_t version = 1, size = sizeof(ExtraIsland), cs = sizeof(Config);
    for (int k = 0; k < T->P.K; k++) extra_island(&T->I[k]);
    int ok = wr(f, "OADNREN1", 8) && wr(f, &version, sizeof version) && wr(f, &size, sizeof size) &&
        wr(f, &cs, sizeof cs) && wr(f, &config, sizeof config) && wr(f, &announced, sizeof announced) &&
        wr(f, ext, sizeof(ExtraIsland) * (size_t)T->P.K);
    for (int k = 0; k < T->P.K && ok; k++) ok = wr(f, T->I[k].mark, sizeof T->I[k].mark);
    return ok;
}

static int extra_load(FILE *f, void *ptr) {
    Terra *T = ptr; char magic[8]; uint32_t v, size, cs; Config saved;
    if (!rd(f, magic, 8) || memcmp(magic, "OADNREN1", 8) || !rd(f, &v, sizeof v) || v != 1 ||
        !rd(f, &size, sizeof size) || size != sizeof(ExtraIsland) || !rd(f, &cs, sizeof cs) || cs != sizeof(Config) ||
        !rd(f, &saved, sizeof saved) || saved.profile < 0 || saved.profile >= NPROFILE ||
        (profile_explicit && saved.profile != config.profile)) {
        fprintf(stderr, "checkpoint sans extension rencontres compatible (import ancien refusé)\n");
        return 0;
    }
    config = saved;
    if (!rd(f, &announced, sizeof announced) || !rd(f, ext, sizeof(ExtraIsland) * (size_t)T->P.K)) return 0;
    for (int k = 0; k < T->P.K; k++) {
        ExtraIsland *e = &ext[k];
        if (e->exchange.n_history < 0 || e->exchange.n_history > CMAX || e->birth.npending < 0 ||
            e->birth.npending > NAISSANCE_EGGS || e->birth.ncooldown < 0 || e->birth.ncooldown > CMAX ||
            !rd(f, T->I[k].mark, sizeof T->I[k].mark)) return 0;
    }
    loading = 1;
    return 1;
}

static void reset_extra(int profile) {
    if (metrics) fclose(metrics);
    metrics = NULL; memset(ext, 0, sizeof ext); config = configuration(profile);
    announced = 0; loading = 0; extension_error = 0;
}

static void close_outputs(Terra *T) {
    if (metrics) { fclose(metrics); metrics = NULL; }
    if (T->series) fclose(T->series);
    if (T->events) fclose(T->events);
    if (T->forces_out) fclose(T->forces_out);
    if (T->societies) fclose(T->societies);
    if (T->attention) fclose(T->attention);
    if (T->traits_out) fclose(T->traits_out);
    if (T->lois_out) fclose(T->lois_out);
}

static int resume_selftest(void) {
    char root[] = "/tmp/octet-rencontres-resume-XXXXXX", full[600], split[600], ck[700];
    if (!mkdtemp(root)) return 1;
    snprintf(full, sizeof full, "%s/full", root); snprintf(split, sizeof split, "%s/split", root);
    if (mkdir(full, 0755) || mkdir(split, 0755)) return 1;
    Params p = defaults(); p.K = 1; p.w = 16; p.h = 8; p.pop0 = 24; p.max_pop = 80;
    p.sample = 100; p.ck_every = 200; p.migrate = 0; p.stress = 1;
    static Terra a, b;
    reset_extra(ALL); terra_new(&a, &p); snprintf(a.dir, sizeof a.dir, "%s", full);
    if (!open_outputs(&a, 0) || terra_run(&a, 400, 1)) return 1;
    uint64_t hash = terra_hash(&a); Rng rng = a.I[0].wr;
    ExtraIsland *expected = malloc(sizeof *expected); if (!expected) return 1;
    *expected = ext[0]; close_outputs(&a); terra_free(&a);
    reset_extra(ALL); terra_new(&b, &p); snprintf(b.dir, sizeof b.dir, "%s", split);
    if (!open_outputs(&b, 0) || terra_run(&b, 200, 1)) return 1;
    close_outputs(&b); terra_free(&b); memset(&b, 0, sizeof b);
    reset_extra(ALL); snprintf(ck, sizeof ck, "%s/state.ck", split);
    if (!load(&b, ck)) return 1;
    snprintf(b.dir, sizeof b.dir, "%s", split);
    if (!open_outputs(&b, 1) || terra_run(&b, 400, 1)) return 1;
    int same = hash == terra_hash(&b) && !memcmp(&rng, &b.I[0].wr, sizeof rng) && !memcmp(expected, &ext[0], sizeof *expected);
    close_outputs(&b); terra_free(&b); free(expected);
    const char *names[] = { "series.tsv", "societies.tsv", "events.tsv", "rencontres.tsv" };
    for (int i = 0; i < 4; i++) {
        char pa[700], pb[700]; snprintf(pa, sizeof pa, "%s/%s", full, names[i]); snprintf(pb, sizeof pb, "%s/%s", split, names[i]);
        char *sa = slurp(pa), *sb = slurp(pb); int eq = sa && sb && !strcmp(sa, sb);
        printf("rencontres reprise : %s %s\n", names[i], eq ? "identique" : "ECHEC");
        same &= eq; free(sa); free(sb);
    }
    printf("rencontres reprise : monde, RNG, œufs, cooldowns, marques %s\n", same ? "OK" : "ECHEC");
    reset_extra(CONTROL);
    return !same;
}

static int rencontres_selftest(void) {
    int xc = echange_selftest(), nc = naissance_selftest(), rv = rendezvous_selftest();
    int rc = xc | nc | rv;
    printf("rencontres modules : échange=%d naissance=%d rendezvous=%d échecs\n", xc, nc, rv);
    /* Neutralité physique : aucun RNG du monde touché lorsque tout est éteint. */
    Params p = defaults(); p.K = 1; p.w = 16; p.h = 8; p.pop0 = 12; p.max_pop = 40;
    p.sample = 100; p.ck_every = 200; p.migrate = 0;
    static Terra a, b;
    reset_extra(CONTROL); terra_new(&a, &p); terra_new(&b, &p);
    for (int t = 0; t < 120; t++) island_tick(&a.I[0]);
    uint64_t ah = terra_hash(&a); Rng ar = a.I[0].wr;
    /* Supprime réellement les trois appels d'extension pour le second run. */
    reset_extra(CONTROL);
    bypass = 1;
    for (int t = 0; t < 120; t++) island_tick(&b.I[0]);
    bypass = 0;
    int same = ah == terra_hash(&b) && !memcmp(&ar, &b.I[0].wr, sizeof ar);
    printf("rencontres désactivées : égalité avec hooks contournés hash+RNG %s ; Params=%zu\n", same ? "OK" : "ECHEC", sizeof(Params));
    rc |= !same; terra_free(&a); terra_free(&b);
    return rc | resume_selftest();
}

static int help(void) {
    fprintf(stderr, "rencontres run --dir CHEMIN_NEUF --profile control|exchange|nursery|meeting|all [options terrarium]\n"
        "    [--exchange-period N] [--exchange-cost N] [--exchange-cooldown N]\n"
        "    [--nursery-period N] [--nursery-cost N] [--nursery-food N] [--nursery-delay N]\n"
        "    [--nursery-season-period N] [--nursery-season-open N] [--nursery-mutation F]\n"
        "    [--meeting-period N] [--meeting-cost N]\n"
        "rencontres selftest | report DIR... | dump DIR\n"
        "Pas d'import state.ck ancien ; pas de copie réelle, migration ou expansion dans cette expérience.\n"
        "Les overrides ci-dessus ne s'appliquent qu'à un --dir neuf : une reprise relit la\n"
        "configuration du checkpoint (même règle que --profile sur une reprise).\n");
    return 2;
}

/* Choix primaire conteste : avant ce commit, period/cost/cooldown/food_cost/delay/
 * season_period/season_open/mutation etaient graves en dur dans configuration(),
 * donc impossibles a balayer sans recompiler trois ELF differents. Ces overrides ne
 * changent aucun defaut : sans drapeau, configuration(profile) reste inchangee. */
typedef struct {
    int has_xp, xp, has_xc, xc, has_xk, xk;
    int has_bp, bp, has_bc, bc, has_bf, bf, has_bd, bd, has_bsp, bsp, has_bso, bso;
    int has_bm; double bm;
    int has_mp, mp, has_mc, mc;
} Overrides;

static void apply_overrides(const Overrides *o) {
    if (o->has_xp) config.exchange.period = o->xp;
    if (o->has_xc) config.exchange.cost = o->xc;
    if (o->has_xk) config.exchange.cooldown = o->xk;
    if (o->has_bp) config.birth.period = o->bp;
    if (o->has_bc) config.birth.cost = o->bc;
    if (o->has_bf) config.birth.food_cost = o->bf;
    if (o->has_bd) config.birth.delay = o->bd;
    if (o->has_bsp) config.birth.season_period = o->bsp;
    if (o->has_bso) config.birth.season_open = o->bso;
    if (o->has_bm) config.birth.mutation = o->bm;
    if (o->has_mp) config.meeting.period = o->mp;
    if (o->has_mc) config.meeting.cost = o->mc;
}

static int protected_world(const char *dir) {
    const char *names[] = { "vie-monde", "vie-forces", "vie-causes", "vie-forces-briques",
        "vie-forces-societes", "vie-attention", "vie-traits", "vie-depense", "vie-causales",
        "vie-saison", "vie-tronc", "vie-lecture" };
    char resolved[4096];
    if (realpath(dir, resolved)) dir = resolved;
    size_t len = strlen(dir);
    while (len && dir[len - 1] == '/') len--;
    size_t start = len;
    while (start && dir[start - 1] != '/') start--;
    for (int i = 0; i < 12; i++)
        if (strlen(names[i]) == len - start && !memcmp(dir + start, names[i], len - start)) return 1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "selftest")) return rencontres_selftest();
    if (argc >= 2 && !strcmp(argv[1], "report")) return cmd_report(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "dump")) return cmd_dump(argc, argv);
    if (argc < 2 || strcmp(argv[1], "run")) return help();
    int profile = ALL; const char *dir = NULL;
    char **args = calloc((size_t)argc + 5, sizeof *args); if (!args) return 1;
    int n = 2; args[0] = argv[0]; args[1] = argv[1];
    Overrides ov; memset(&ov, 0, sizeof ov);
    for (int i = 2; i < argc; i += 2) {
        if (i + 1 == argc) { free(args); return help(); }
        const char *a = argv[i], *v = argv[i + 1];
        if (!strcmp(a, "--profile")) {
            profile = -1; profile_explicit = 1;
            for (int j = 0; j < NPROFILE; j++) if (!strcmp(v, profiles[j])) profile = j;
            if (profile < 0) { free(args); return help(); }
        } else if (!strcmp(a, "--exchange-period")) { ov.has_xp = 1; ov.xp = atoi(v); }
        else if (!strcmp(a, "--exchange-cost")) { ov.has_xc = 1; ov.xc = atoi(v); }
        else if (!strcmp(a, "--exchange-cooldown")) { ov.has_xk = 1; ov.xk = atoi(v); }
        else if (!strcmp(a, "--nursery-period")) { ov.has_bp = 1; ov.bp = atoi(v); }
        else if (!strcmp(a, "--nursery-cost")) { ov.has_bc = 1; ov.bc = atoi(v); }
        else if (!strcmp(a, "--nursery-food")) { ov.has_bf = 1; ov.bf = atoi(v); }
        else if (!strcmp(a, "--nursery-delay")) { ov.has_bd = 1; ov.bd = atoi(v); }
        else if (!strcmp(a, "--nursery-season-period")) { ov.has_bsp = 1; ov.bsp = atoi(v); }
        else if (!strcmp(a, "--nursery-season-open")) { ov.has_bso = 1; ov.bso = atoi(v); }
        else if (!strcmp(a, "--nursery-mutation")) { ov.has_bm = 1; ov.bm = atof(v); }
        else if (!strcmp(a, "--meeting-period")) { ov.has_mp = 1; ov.mp = atoi(v); }
        else if (!strcmp(a, "--meeting-cost")) { ov.has_mc = 1; ov.mc = atoi(v); }
        else {
            if (!strcmp(a, "--dir")) dir = v;
            if ((!strcmp(a, "--migrate") || !strcmp(a, "--expand-every")) && strcmp(v, "0")) { free(args); return help(); }
            if (!strcmp(a, "--forces") && atoi(v) == 3) { free(args); return help(); }
            if (!strcmp(a, "--repro") && strcmp(v, "split")) { free(args); return help(); }
            args[n++] = argv[i]; args[n++] = argv[i + 1];
        }
    }
    if (!dir || strlen(dir) > 480) { free(args); return help(); }
    if (protected_world(dir)) { fprintf(stderr, "répertoire d'un des douze anciens mondes refusé : %s\n", dir); free(args); return 1; }
    /* Les identités changent à migration et expansion ; cette première physique
     * garde les îles fixes pour préserver cooldowns et parentés sans ambiguïté. */
    args[n++] = "--migrate"; args[n++] = "0"; args[n] = NULL;
    reset_extra(profile);
    /* Fige les constantes avant le premier tick ; sans effet sur une reprise, où
     * extra_load() relit la configuration sauvegardée (même règle que --profile). */
    apply_overrides(&ov);
    if (mkdir(dir, 0755) && access(dir, F_OK)) { perror(dir); free(args); return 1; }
    char lockpath[600]; snprintf(lockpath, sizeof lockpath, "%s/rencontres.lock", dir);
    int lock = open(lockpath, O_CREAT | O_RDWR, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) { fprintf(stderr, "monde déjà ouvert ou verrou impossible : %s\n", dir); free(args); return 1; }
    int rc = cmd_run(n, args);
    if (metrics) { fclose(metrics); metrics = NULL; }
    close(lock); free(args);
    return rc || extension_error;
}
