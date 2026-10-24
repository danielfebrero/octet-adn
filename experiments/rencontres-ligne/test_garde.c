/* Tests reproductibles de garde : aucune lecture du vrai /proc.
 * Compiler vers un chemin neuf, puis : test_garde /chemin/neuf/garde
 * Tous les fichiers et liens de fixture restent dans un mkdtemp privé.
 * Nettoyage sur succès ; répertoire conservé sur échec pour diagnostic. */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *names[] = {
    "vie-monde", "vie-forces", "vie-causes", "vie-forces-briques",
    "vie-forces-societes", "vie-attention", "vie-traits", "vie-depense",
    "vie-causales", "vie-saison", "vie-tronc", "vie-lecture"
};
static const char *guard;
static char base[128], procroot[160];
static int failures, cases;

static void die(const char *what) {
    perror(what);
    if (*base) fprintf(stderr, "fixtures conservées : %s\n", base);
    exit(2);
}
static FILE *create(const char *path, const char *mode) {
    FILE *f = fopen(path, mode);
    if (!f) die(path);
    return f;
}
static void finish(FILE *f) {
    int bad = ferror(f);
    if (fclose(f)) bad = 1;
    if (bad) die("écriture fixture");
}
static void makeproc(int pid, const char *name, long start, char state, int ppid) {
    char path[256], run_dir[200], image[256];
    snprintf(path, sizeof path, "%s/%d", procroot, pid);
    if (mkdir(path, 0700) && errno != EEXIST) die(path);
    snprintf(path, sizeof path, "%s/%d/cmdline", procroot, pid);
    FILE *f = create(path, "wb");
    fputs("fixture_terrarium", f); fputc(0, f);
    fputs("--dir", f); fputc(0, f);
    snprintf(run_dir, sizeof run_dir, "%s/run/%s", base, name);
    fputs(run_dir, f); fputc(0, f); finish(f);
    snprintf(path, sizeof path, "%s/%d/stat", procroot, pid);
    f = create(path, "w");
    fprintf(f, "%d (name with ) paren) %c %d", pid, state, ppid);
    for (int i = 0; i < 17; i++) fputs(" 0", f);
    fprintf(f, " %ld\n", start); finish(f);
    snprintf(image, sizeof image, "%s/image-%d", base, pid);
    f = fopen(image, "wx");
    if (f) { fputs("fixture ELF", f); finish(f); }
    else if (errno != EEXIST) die(image);
    snprintf(path, sizeof path, "%s/%d/exe", procroot, pid);
    if (symlink(image, path) && errno != EEXIST) die(path);
}
static void run(const char *label, const char *root, const char *op,
                const char *proof, int expected_exit, int absent_after) {
    char logfile[256];
    snprintf(logfile, sizeof logfile, "%s/case-%02d.log", base, ++cases);
    fflush(stdout);
    pid_t child = fork();
    if (child < 0) die("fork");
    if (!child) {
        int logfd = open(logfile, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (logfd < 0 || dup2(logfd, STDOUT_FILENO) < 0 ||
            dup2(logfd, STDERR_FILENO) < 0) _exit(126);
        close(logfd);
        execl(guard, guard, "--proc", root, op, proof, (char *)NULL);
        _exit(127);
    }
    int status;
    while (waitpid(child, &status, 0) < 0) if (errno != EINTR) die("waitpid");
    int actual = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    int pass = actual == expected_exit;
    if (absent_after) {
        struct stat st;
        if (!lstat(proof, &st) || errno != ENOENT) pass = 0;
    }
    if (!pass) failures++;
    printf("%02d %s : %s (exit=%d, attendu=%d)\n", cases, label,
           pass ? "PASS" : "FAIL", actual, expected_exit);
    if (!pass) {
        FILE *f = fopen(logfile, "r");
        if (f) { int c; while ((c = fgetc(f)) != EOF) fputc(c, stderr); fclose(f); }
    }
}
/* lstat empêche de suivre les liens exe ; aucun chemin hors mkdtemp n'est effacé. */
static void cleanup(const char *path) {
    struct stat st;
    if (lstat(path, &st)) die(path);
    if (!S_ISDIR(st.st_mode)) { if (unlink(path)) die(path); return; }
    DIR *dp = opendir(path);
    if (!dp) die(path);
    struct dirent *de;
    while ((de = readdir(dp))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char next[1024];
        int n = snprintf(next, sizeof next, "%s/%s", path, de->d_name);
        if (n < 0 || (size_t)n >= sizeof next) { errno = ENAMETOOLONG; die(path); }
        cleanup(next);
    }
    if (closedir(dp) || rmdir(path)) die(path);
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: test_garde /chemin/neuf/garde\n"); return 2; }
    guard = argv[1];
    if (access(guard, X_OK)) die(guard);
    char tmp[] = "/tmp/octet-garde-fixture-XXXXXX";
    char *dir = mkdtemp(tmp);
    if (!dir) die("mkdtemp");
    strcpy(base, dir);
    snprintf(procroot, sizeof procroot, "%s/proc", base);
    if (mkdir(procroot, 0700)) die(procroot);
    for (int i = 0; i < 12; i++) makeproc(101 + i, names[i], (101 + i) * 100, 'S', 1);
    char proof[256], proof2[256], refused[256], path[256], empty[160];
    snprintf(proof, sizeof proof, "%s/snapshot.txt", base);
    run("snapshot complet", procroot, "snapshot", proof, 0, 0);
    run("check identique", procroot, "check", proof, 0, 0);
    run("preuve existante refusée", procroot, "snapshot", proof, 1, 0);
    makeproc(101, names[0], 99999, 'S', 1);
    run("réutilisation PID refusée", procroot, "check", proof, 1, 0);
    makeproc(101, names[0], 10100, 'S', 1);
    snprintf(path, sizeof path, "%s/image-101", base);
    FILE *f = create(path, "w"); fputs("image de fixture modifiée", f); finish(f);
    run("ELF modifié refusé", procroot, "check", proof, 1, 0);
    snprintf(proof2, sizeof proof2, "%s/snapshot2.txt", base);
    run("nouveau snapshot", procroot, "snapshot", proof2, 0, 0);
    makeproc(101, names[0], 10100, 'Z', 1);
    run("zombie refusé", procroot, "check", proof2, 1, 0);
    makeproc(101, names[0], 10100, 'S', 7);
    run("PPID refusé", procroot, "check", proof2, 1, 0);
    makeproc(101, names[0], 10100, 'S', 1);
    makeproc(999, names[0], 99900, 'S', 1);
    snprintf(refused, sizeof refused, "%s/refused-duplicate.txt", base);
    run("doublon refusé sans preuve", procroot, "snapshot", refused, 1, 1);
    snprintf(path, sizeof path, "%s/999/cmdline", procroot);
    f = create(path, "wb"); fputc(0, f); finish(f);
    snprintf(path, sizeof path, "%s/102/cmdline", procroot);
    f = create(path, "wb"); fputc(0, f); finish(f);
    snprintf(refused, sizeof refused, "%s/refused-missing.txt", base);
    run("absent refusé sans preuve", procroot, "snapshot", refused, 1, 1);
    snprintf(empty, sizeof empty, "%s/empty-proc", base);
    if (mkdir(empty, 0700)) die(empty);
    snprintf(refused, sizeof refused, "%s/refused-empty.txt", base);
    run("namespace vide refusé sans preuve", empty, "snapshot", refused, 1, 1);
    printf("garde fixtures : %d cas, %d échec(s)\n", cases, failures);
    if (failures) fprintf(stderr, "fixtures conservées : %s\n", base);
    else cleanup(base);
    return failures ? 1 : 0;
}
