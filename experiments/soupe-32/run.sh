#!/bin/bash
# 32 graines. Phrase et portes : README.md, écrites avant le compte.
# Même binaire que soup-replicator : addr b, N=16384, 16384 époques, report 256.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
mkdir -p experiments/soupe-32/runs
for s in $(seq 0 31); do
    out=$(printf 'experiments/soupe-32/runs/seed-%02d.txt' "$s")
    ./octet-adn soup --addr b --programs 16384 --ticks 16384 --report 256 --seed "$s" > "$out" &
done
fail=0
wait || fail=1
{
    echo -e "graine\tt_first\tt_half\tmin_essentiels\tcopieurs_fin\tnaissances\tpasse"
    n=0
    for s in $(seq 0 31); do
        out=$(printf 'experiments/soupe-32/runs/seed-%02d.txt' "$s")
        line=$(grep '^verdict ' "$out" || true)
        if [ -z "$line" ]; then
            echo -e "$s\t.\t.\t.\t.\t.\t0"
            continue
        fi
        t_first=$(printf '%s\n' "$line" | sed -n 's/.*t_first=\([^ ]*\).*/\1/p')
        t_half=$(printf '%s\n' "$line" | sed -n 's/.*t_half=\([^ ]*\).*/\1/p')
        ess=$(printf '%s\n' "$line" | sed -n 's/.*min_essentiels=\([^ ]*\).*/\1/p')
        cop=$(printf '%s\n' "$line" | sed -n 's/.*copieurs_fin=\([^ ]*\).*/\1/p')
        births=$(printf '%s\n' "$line" | sed -n 's/.*naissances=\([^ ]*\).*/\1/p')
        passe=0
        case "$line" in
            *"réplicateur OUI"*) passe=1; n=$((n + 1));;
        esac
        echo -e "$s\t$t_first\t$t_half\t$ess\t$cop\t$births\t$passe"
    done
    echo "passent : $n/32"
} > experiments/soupe-32/results.txt
cat experiments/soupe-32/results.txt
exit "$fail"
