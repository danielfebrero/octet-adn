/* Garde avant/après : lecture de /proc uniquement, jamais de signal ni ELF écrit.
 * cc -std=c11 -O2 -Wall -Wextra -Werror garde.c -o /CHEMIN/NEUF/garde
 * garde [--proc DIR] snapshot FILE | check FILE
 * --proc sert aux fixtures ; seule la racine /proc décrit le namespace courant.
 * Une absence dans ce namespace n'est pas une vérification de l'hôte distant.
 * Snapshot exclusif (FILE doit être absent), seulement après 14 mondes valides.
 * Format : OCTET_GARDE_V1 14, puis 14 lignes séparées par tabulations :
 * nom pid starttime ppid dev inode taille mtime_sec mtime_nsec ctime_sec
 * ctime_nsec dir_hex. check exige aussi l'absence de doublons et PPID=1. */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mondes.h"

enum { NW = OCTET_NMONDES, PATH_CAP = 4096, CMD_CAP = 65536 };
#define names octet_mondes
typedef struct {
    long pid, ppid;
    uint64_t start;
    uintmax_t dev, ino;
    intmax_t size, mt, ct;
    long mn, cn;
    char state, dir[PATH_CAP];
} Record;

static int error(const char *msg, const char *detail) {
    fprintf(stderr, "garde: %s%s%s\n", msg, detail ? ": " : "", detail ? detail : "");
    return -1;
}
static int numeric(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}
static int world_index(const char *path) {
    size_t n = strlen(path);
    while (n && path[n - 1] == '/') n--;
    size_t start = n;
    while (start && path[start - 1] != '/') start--;
    for (int i = 0; i < NW; i++)
        if (strlen(names[i]) == n - start && !memcmp(names[i], path + start, n - start)) return i;
    return -1;
}
static int read_at(int fd, const char *name, char *buf, size_t cap) {
    int f = openat(fd, name, O_RDONLY | O_CLOEXEC);
    if (f < 0) return -1;
    size_t n = 0;
    while (n < cap - 1) {
        ssize_t got = read(f, buf + n, cap - 1 - n);
        if (got < 0 && errno == EINTR) continue;
        if (got < 0) { close(f); return -1; }
        if (!got) { close(f); buf[n] = 0; return (int)n; }
        n += (size_t)got;
    }
    close(f); errno = EOVERFLOW; return -1;
}
static int process_stat(int fd, Record *r) {
    char buf[8192], *end, *save = NULL;
    if (read_at(fd, "stat", buf, sizeof buf) < 0) return -1;
    char *tail = strrchr(buf, ')');
    if (!tail) return -1;
    char *token = strtok_r(tail + 1, " \t\n", &save);
    for (int field = 3; field <= 22; field++) {
        if (!token) return -1;
        if (field == 3) { if (strlen(token) != 1) return -1; r->state = token[0]; }
        if (field == 4) {
            errno = 0; r->ppid = strtol(token, &end, 10);
            if (errno || *end || r->ppid < 0) return -1;
        }
        if (field == 22) {
            errno = 0; r->start = strtoull(token, &end, 10);
            if (errno || *end || *token == '-') return -1;
        }
        token = strtok_r(NULL, " \t\n", &save);
    }
    return 0;
}
static int same(const Record *a, const Record *b) {
    return a->pid == b->pid && a->start == b->start && a->ppid == b->ppid &&
        a->dev == b->dev && a->ino == b->ino && a->size == b->size &&
        a->mt == b->mt && a->mn == b->mn && a->ct == b->ct && a->cn == b->cn &&
        !strcmp(a->dir, b->dir);
}
static int scan(const char *root, Record out[NW]) {
    DIR *dp = opendir(root);
    int seen[NW] = {0}, failures = 0;
    if (!dp) return error("racine proc illisible", root);
    struct dirent *de;
    for (;;) {
        errno = 0; de = readdir(dp);
        if (!de) { if (errno) { error("parcours proc incomplet", root); failures++; } break; }
        if (!numeric(de->d_name)) continue;
        int fd = openat(dirfd(dp), de->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            if (errno != ENOENT && errno != ESRCH) { error("processus illisible", de->d_name); failures++; }
            continue;
        }
        char cmd[CMD_CAP];
        int n = read_at(fd, "cmdline", cmd, sizeof cmd);
        if (n < 0) {
            if (errno != ENOENT && errno != ESRCH) { error("cmdline illisible", de->d_name); failures++; }
            close(fd); continue;
        }
        if (n && cmd[n - 1]) { error("cmdline non terminée", de->d_name); failures++; close(fd); continue; }
        int dir_args = 0, matched = 0;
        for (int pos = 0; pos < n;) {
            char *arg = cmd + pos;
            pos += (int)strlen(arg) + 1;
            if (strcmp(arg, "--dir") || pos >= n) continue;
            dir_args++;
            const char *dir = cmd + pos;
            pos += (int)strlen(dir) + 1;
            int i = world_index(dir);
            if (i < 0) continue;
            matched++;
            if (seen[i]++) { error("monde dupliqué", names[i]); failures++; continue; }
            Record r = {0}, after = {0}; struct stat st;
            char *pid_end;
            errno = 0; r.pid = strtol(de->d_name, &pid_end, 10);
            if (errno || *pid_end || r.pid <= 0 || strlen(dir) >= sizeof r.dir ||
                process_stat(fd, &r) || fstatat(fd, "exe", &st, 0) ||
                !S_ISREG(st.st_mode) || process_stat(fd, &after)) {
                error("stat/exe illisible ou invalide", names[i]); failures++; continue;
            }
            if (r.start != after.start || r.ppid != after.ppid || r.ppid != 1 ||
                r.state == 'Z' || r.state == 'X' || r.state == 'x' ||
                after.state == 'Z' || after.state == 'X' || after.state == 'x') {
                error("processus changé, mort ou PPID différent de 1", names[i]); failures++; continue;
            }
            strcpy(r.dir, dir);
            r.dev = st.st_dev; r.ino = st.st_ino; r.size = st.st_size;
            r.mt = st.st_mtim.tv_sec; r.mn = st.st_mtim.tv_nsec;
            r.ct = st.st_ctim.tv_sec; r.cn = st.st_ctim.tv_nsec;
            out[i] = r;
        }
        if (matched && dir_args > 1) { error("plusieurs --dir, processus ambigu", de->d_name); failures++; }
        close(fd);
    }
    closedir(dp);
    for (int i = 0; i < NW; i++) if (!seen[i]) { error("monde absent", names[i]); failures++; }
    return failures ? -1 : 0;
}
static void print_record(FILE *f, int i, const Record *r) {
    fprintf(f, "%s\t%ld\t%" PRIu64 "\t%ld\t%" PRIuMAX "\t%" PRIuMAX
        "\t%" PRIdMAX "\t%" PRIdMAX "\t%ld\t%" PRIdMAX "\t%ld\t",
        names[i], r->pid, r->start, r->ppid, r->dev, r->ino, r->size, r->mt, r->mn, r->ct, r->cn);
    for (const unsigned char *s = (const unsigned char *)r->dir; *s; s++) fprintf(f, "%02x", *s);
    fputc('\n', f);
}
static int save_file(const char *file, Record rows[NW]) {
    FILE *f = fopen(file, "wx");
    if (!f) return error("snapshot non créé (existe déjà ou écriture refusée)", file);
    fprintf(f, "OCTET_GARDE_V1\t%d\n", NW);
    for (int i = 0; i < NW; i++) print_record(f, i, &rows[i]);
    int bad = ferror(f);
    if (fclose(f)) bad = 1;
    if (bad) { unlink(file); return error("écriture snapshot incomplète", file); }
    return 0;
}
static int unhex(char *dst, const char *src) {
    size_t n = strlen(src);
    if (!n || n % 2 || n / 2 >= PATH_CAP) return -1;
    for (size_t i = 0; i < n; i += 2) {
        char pair[3] = {src[i], src[i + 1], 0}, *end;
        if (!isxdigit((unsigned char)pair[0]) || !isxdigit((unsigned char)pair[1])) return -1;
        long v = strtol(pair, &end, 16);
        if (*end || !v) return -1;
        dst[i / 2] = (char)v;
    }
    dst[n / 2] = 0; return 0;
}
static int load_file(const char *file, Record rows[NW]) {
    FILE *f = fopen(file, "r");
    if (!f) return error("snapshot illisible", file);
    char line[PATH_CAP * 2 + 1024], name[64], hex[PATH_CAP * 2], extra;
    int seen[NW] = {0}, bad = 0;
    char header[64]; snprintf(header, sizeof header, "OCTET_GARDE_V1\t%d\n", NW);
    if (!fgets(line, sizeof line, f) || strcmp(line, header)) bad = 1;
    for (int k = 0; !bad && k < NW; k++) {
        Record r = {0};
        if (!fgets(line, sizeof line, f) || sscanf(line, "%63s %ld %" SCNu64
            " %ld %" SCNuMAX " %" SCNuMAX " %" SCNdMAX " %" SCNdMAX
            " %ld %" SCNdMAX " %ld %8191s %c", name, &r.pid, &r.start, &r.ppid,
            &r.dev, &r.ino, &r.size, &r.mt, &r.mn, &r.ct, &r.cn, hex, &extra) != 12) { bad = 1; break; }
        int i = -1;
        for (int j = 0; j < NW; j++) if (!strcmp(names[j], name)) i = j;
        if (i < 0 || seen[i]++ || r.pid <= 0 || r.ppid != 1 || r.size < 0 ||
            r.mn < 0 || r.mn >= 1000000000L || r.cn < 0 || r.cn >= 1000000000L ||
            unhex(r.dir, hex) || world_index(r.dir) != i) { bad = 1; break; }
        rows[i] = r;
    }
    if (ferror(f) || fgetc(f) != EOF) bad = 1;
    fclose(f);
    return bad ? error("snapshot invalide ou incomplet", file) : 0;
}
int main(int argc, char **argv) {
    const char *root = "/proc";
    if (argc >= 3 && !strcmp(argv[1], "--proc")) { root = argv[2]; argv += 2; argc -= 2; }
    if (argc != 3 || (strcmp(argv[1], "snapshot") && strcmp(argv[1], "check"))) {
        fprintf(stderr, "usage: garde [--proc DIR] snapshot FILE | check FILE\n"); return 2;
    }
    Record current[NW] = {{0}}, old[NW] = {{0}};
    int snapshot = !strcmp(argv[1], "snapshot");
    if ((!snapshot && load_file(argv[2], old)) || scan(root, current)) return 1;
    if (snapshot) { if (save_file(argv[2], current)) return 1; }
    else for (int i = 0; i < NW; i++) if (!same(&old[i], &current[i])) {
        error("PID/starttime/exe/dir modifié depuis snapshot", names[i]); return 1;
    }
    printf("garde %s OK : %d processus vivants, PPID 1 ; proc=%s\n", argv[1], NW, root);
    for (int i = 0; i < NW; i++)
        printf("%s pid=%ld start=%" PRIu64 " exe=%" PRIuMAX ":%" PRIuMAX
               " size=%" PRIdMAX " state=%c dir=%s\n", names[i], current[i].pid,
               current[i].start, current[i].dev, current[i].ino, current[i].size,
               current[i].state, current[i].dir);
    return 0;
}
