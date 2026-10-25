#!/bin/sh
# Analyse préenregistrée (SENESCENCE-BRAS.md). awk seul : pas de python ici.
set -eu
root=$1
for d in "$root"/*-1*; do
  [ -d "$d" ] || continue
  b=$(basename "$d"); arm=${b%-*}; seed=${b##*-}
  awk -F'\t' -v arm="$arm" -v seed="$seed" 'NR > 1 && $1 > 80000 && $1 <= 100000 {
      n += $27; l += $27 * $28; a += $27 * $30; c += $27 * $31; e += $27 * $32; f += $27 * $33; lo += $27 * $29; pop = $2 }
    END { if (n > 0) printf "%s\t%s\t%d\t%d\t%.3f\t%.4f\t%.3f\t%.3f\t%.3f\t%.3f\n", seed, arm, pop, n, l/n, lo/n, a/n, c/n, e/n, f/n
          else printf "%s\t%s\t%d\t0\tNA\tNA\tNA\tNA\tNA\tNA\n", seed, arm, pop }' "$d/vivre.tsv"
done | sort > "$root/mondes.tsv"
echo "graine	bras	pop	naissances	M	part_opcode	n1	n4	n6	n10"; cat "$root/mondes.tsv"
awk -F'\t' '{ v[$1, $2] = $5; for (j = 7; j <= 10; j++) z[$1, $2, j] = $j; s[$1] = 1 }
  function ci(name, k, d, ss, m, sd, h, i, x, n) {
    n = 0; m = 0
    for (i in s) if ((i, "senescence-decouple") in v && v[i, "senescence-decouple"] != "NA" && v[i, k] != "NA") { x[++n] = (col ? z[i, k, col] - z[i, "senescence-decouple", col] : v[i, k] - v[i, "senescence-decouple"]); m += x[n] }
    if (n < 2) { printf "%s : n=%d, insuffisant\n", name, n; return }
    m /= n; ss = 0; for (i = 1; i <= n; i++) ss += (x[i] - m) ^ 2; sd = sqrt(ss / (n - 1))
    h = (n == 20 ? 2.433 : 2.5) * sd / sqrt(n)   # t(0,9875 ; 19 ddl) ; autre n : approximation prudente
    printf "%s : n=%d moyenne=%.2f IC97,5%%=[%.2f ; %.2f]\n", name, n, m, m - h, m + h
  }
  END { col = 0; ci("D1 actuel − découplé (prédit > 0)", "senescence"); ci("D2 inversé − découplé (prédit < 0)", "senescence-inverse")
        split("n1 n4 n6 n10", nm, " ")
        for (col = 7; col <= 10; col++) { ci("diag " nm[col-6] " actuel − découplé", "senescence"); ci("diag " nm[col-6] " inversé − découplé", "senescence-inverse") } }' "$root/mondes.tsv"
