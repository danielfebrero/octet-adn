#!/bin/sh
# Analyse préenregistrée Q2 (PROTOCOLE.md). awk seul.
set -eu
root=$1
for d in "$root"/?-1*; do
  [ -d "$d" ] || continue
  b=$(basename "$d"); arm=${b%%-*}; seed=${b#*-}
  awk -F'\t' -v arm="$arm" -v seed="$seed" '
    FNR == 1 { next }
    FILENAME ~ /naissances/ {
      if ($4 == 2 && $10 >= 0 && $11 >= 0) { pe += $10; re += $11; ne++ }
      if ($4 == 1 && $10 >= 0 && $11 >= 0) { pf += $10; rf += $11; nf++ }
      dep[$2] = ($3 > 0 && ($3 in dep) && $10 >= 0 && $11 >= 0 && $10 < $11) ? dep[$3] + 1 : 0
      L += dep[$2]; nb++
      next }
    { k++; e[k % 8] = $2; g[k % 8] = $3; o[k % 8] = $5; last = $0 }
    END { for (i = 0; i < 8; i++) { E += e[i]; G += g[i]; O += o[i] }
          split(last, f, "\t")
          printf "%s\t%s\t%s\t%.4f\t%s\t%d\t%.2f\t%.2f\t%.3f\t%s\t%s\n", seed, arm, ne ? sprintf("%.4f", 1 - pe / re) : "NA", nb ? L / nb : 0,
                 nf ? sprintf("%.4f", 1 - pf / rf) : "NA", ne, E / 8, G / 8, O / 8, f[11], f[12] }' "$d/naissances.tsv" "$d/mesures.tsv"
done | sort > "$root/mondes.tsv"
echo "graine	bras	H_e	L	H_fission	nes_empreinte	ensembles	plus_grand	ordre	gen_max	projections"; cat "$root/mondes.tsv"
awk -F'\t' '{ h[$1, $2] = $3; l[$1, $2] = $4; o[$1, $2] = $9; s[$1] = 1 }
  function ci(name, A, B, kind,   i, n, m, ss, sd, hw, x, a, b, pos) {
    n = 0; m = 0; pos = 0
    for (i in s) { a = kind == "h" ? h[i, A] : kind == "l" ? l[i, A] : o[i, A]; b = kind == "h" ? h[i, B] : kind == "l" ? l[i, B] : o[i, B]
      if (a == "" || b == "" || a == "NA" || b == "NA") continue; x[++n] = a - b; m += x[n]; pos += x[n] > 0 }
    if (n < 2) { printf "%s : n=%d insuffisant\n", name, n; return }
    m /= n; ss = 0; for (i = 1; i <= n; i++) ss += (x[i] - m) ^ 2; sd = sqrt(ss / (n - 1))
    hw = (n == 20 ? 2.433 : 2.6) * sd / sqrt(n)
    printf "%s : n=%d moyenne=%.4f IC97,5%%=[%.4f ; %.4f] positifs=%d/%d\n", name, n, m, m - hw, m + hw, pos, n }
  END { ci("C1 H_e(E) − H_e(H)", "E", "H", "h"); ci("C2 L(E) − L(H)", "E", "H", "l")
        ci("diag H_e(E) − H_e(U)", "E", "U", "h"); ci("diag L(E) − L(U)", "E", "U", "l"); ci("diag L(E) − L(S)", "E", "S", "l")
        ci("diag ordre(E) − ordre(H)", "E", "H", "o"); ci("diag ordre(E) − ordre(S)", "E", "S", "o") }' "$root/mondes.tsv"
