#!/bin/sh
# Analyse préenregistrée (PROTOCOLE.md). Évaluation finale en lecture seule de chaque checkpoint, puis contrastes.
set -eu
elf=$1; root=$2
for d in "$root"/*-*-1*; do [ -d "$d" ] && "$elf" eval "$d"; done | sort > "$root/eval-final.tsv"
echo "milieu	bras	graine	cycle	pop	m0	m1	m2	m3	episodes"; cat "$root/eval-final.tsv"
awk -F'\t' '
  function prim(t, m0, m1, m2) {
    if (t == "observation" || t == "patience") return m1 - m2
    if (t == "chainage") return m0 - 0.5 * m1
    return m0 }
  { p[$1, $2, $3] = prim($1, $6, $7, $8); task[$1] = 1; seed[$3] = 1 }
  END {
    split("memoire composition inversion observation patience chainage transfert signal autrui relation", order, " ")
    print ""; print "milieu\tP_info\tP_temoin\tdelta\tIC99,5_bas\tIC99,5_haut\tpositifs\tverdict"
    for (k = 1; k <= 10; k++) { t = order[k]; n = 0; m = 0; pi = 0; pt = 0; pos = 0
      for (s in seed) if ((t, "info", s) in p && (t, "temoin", s) in p) { x[++n] = p[t, "info", s] - p[t, "temoin", s]; m += x[n]; pi += p[t, "info", s]; pt += p[t, "temoin", s]; pos += x[n] > 0 }
      if (n != 10) { printf "%s\tn=%d incomplet (10 graines requises), aucun verdict\n", t, n; continue }
      m /= n; ss = 0; for (i = 1; i <= n; i++) ss += (x[i] - m) ^ 2; sd = sqrt(ss / (n - 1))
      h = 3.690 * sd / sqrt(n)
      printf "%s\t%.3f\t%.3f\t%+.3f\t%+.3f\t%+.3f\t%d/%d\t%s\n", t, pi / n, pt / n, m, m - h, m + h, pos, n, (m - h > 0 ? "TENU" : "non") } }' "$root/eval-final.tsv"
