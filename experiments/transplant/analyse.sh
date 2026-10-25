#!/bin/sh
# Analyse préenregistrée (PROTOCOLE.md), awk seul. Lit la dernière ligne de chaque origine.tsv.
set -eu
root=$1
for d in "$root"/[ABC]-*; do
  [ -f "$d/origine.tsv" ] || continue
  printf "%s\t%s\t%s\n" "$(basename "$d")" "$(sed -n 2p "$d/origine.tsv")" "$(tail -1 "$d/origine.tsv")"
done > "$root/fins.tsv"
awk -F'\t' '
  { name = $1; a = $8; b = $9; u = $10; pop = $11; t0 = $2; t1 = $7
    n = split(name, f, "-") ; seed = f[n]
    tot = a + b + u; share = tot ? a / tot : -1; bshare = tot ? b / tot : -1 }
  name ~ /^A-/ { m = name; sub(/-[0-9]+$/, "", m); key = m; sA[key, seed] = share; seedset[seed] = 1; akeys[key] = 1 }
  name ~ /^B-/ { m = name; sub(/-[0-9]+$/, "", m); bsum[m] += share; bn[m]++; if (share > .5) bwin[m]++; if (share == 0 || share == 1) bfix[m]++ }
  name ~ /^C-/ { m = name; sub(/-[0-9]+$/, "", m); cB[m, seed] = bshare; cpop[m, seed] = pop; cseed[seed] = 1 }
  END {
    print "== A. Jardin réciproque (s_X = part de X à la fin) =="
    split("vie-monde:vie-forces vie-monde:vie-causes vie-forces:vie-causes", pairs, " ")
    for (k = 1; k <= 3; k++) { split(pairs[k], xy, ":"); X = xy[1]; Y = xy[2]; pos = 0; nn = 0; sum = 0
      kx = "A-" X "-" Y "-hote-" X; ky = "A-" X "-" Y "-hote-" Y
      for (s in seedset) if ((kx, s) in sA && (ky, s) in sA && sA[kx, s] >= 0 && sA[ky, s] >= 0) {
        la = sA[kx, s] - sA[ky, s]; nn++; sum += la; pos += la > 0; sx += sA[kx, s]; sy += sA[ky, s] }
      printf "%s contre %s : LA moyen %+.3f, LA>0 dans %d/%d graines, s_X chez X %.3f, chez Y %.3f -> %s\n", X, Y, nn ? sum / nn : 0, pos, nn,
             nn ? sx / nn : 0, nn ? sy / nn : 0, (nn == 10 && pos >= 9) ? "TENU (adaptation locale)" : (nn == 10 ? "non" : "incomplet")
      sx = sy = 0 }
    split("vie-monde vie-forces vie-causes", ws, " ")
    for (k = 1; k <= 3; k++) { key = "A-" ws[k] "-" ws[k] "-hote-" ws[k]; lo = 1; hi = 0; nn = 0; sum = 0
      for (s in seedset) if ((key, s) in sA) { v = sA[key, s]; nn++; sum += v; if (v < lo) lo = v; if (v > hi) hi = v }
      printf "dérive %s contre lui-même : part moyenne %.3f [%.3f ; %.3f] sur %d\n", ws[k], nn ? sum / nn : 0, lo, hi, nn }
    print "== B. Tournoi famille division (part de la première origine nommée) =="
    for (m in bn) printf "%s : part moyenne %.3f, gagne %d/%d, fixation %d\n", m, bsum[m] / bn[m], bwin[m], bn[m], bfix[m]
    print "== C. Téléportation =="
    split("vie-monde vie-causes", src, " ")
    for (k = 1; k <= 2; k++) { S = src[k]; pos = 0; nn = 0; dm = 0; me = 0; et = 0
      for (s in cseed) { km = "C-" S "-meme"; ke = "C-" S "-etranger"
        if ((km, s) in cB && (ke, s) in cB) { d = cB[km, s] - cB[ke, s]; nn++; dm += d; pos += d > 0; me += cB[km, s]; et += cB[ke, s] } }
      printf "%s : arrivants même monde %.3f, étrangers %.3f, écart %+.3f, >0 dans %d/%d -> %s\n", S, nn ? me / nn : 0, nn ? et / nn : 0, nn ? dm / nn : 0, pos, nn,
             (nn == 10 && pos >= 9) ? "TENU (résistance à l\x27étranger)" : (nn == 10 ? "non" : "incomplet")
      rt = 0; rn = 0; for (s in cseed) if (("C-" S "-temoin", s) in cpop && ("C-" S "-retrait", s) in cpop) { rt += cpop["C-" S "-retrait", s] / cpop["C-" S "-temoin", s]; rn++ }
      printf "%s : population finale après retrait de moitié / témoin = %.3f (sur %d)\n", S, rn ? rt / rn : 0, rn } }' "$root/fins.tsv"
