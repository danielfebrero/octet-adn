#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d /tmp/octet-0996-validation-build.XXXXXXXX)
proof_dir=$(mktemp -d /tmp/octet-0996-validation-preuves.XXXXXXXX)
core_sources='src/rng.c src/isa.c src/vm.c src/mutate.c src/world.c src/labs.c src/store.c src/runner.c src/seeds.S src/labs_h.c src/novelty.c src/soup.c'
# Les expansions non citées ici sont des listes constantes de fichiers du dépôt.
cc -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Iinclude \
    -pthread -o "$build_dir/terrarium" experiments/rencontres.c \
    experiments/rencontres-ligne/echange.c experiments/rencontres-ligne/naissance.c \
    experiments/rencontres-ligne/rendezvous.c $core_sources -lm -ldl
cc -O2 -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-format-truncation -Iinclude -Iexperiments/rencontres-ligne \
    -o "$build_dir/diagnostic" experiments/rencontres-ligne/diagnostic.c \
    experiments/rencontres-ligne/rendezvous.c experiments/rencontres-ligne/naissance.c \
    $core_sources -lm
cc -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Iinclude \
    -o "$build_dir/historiques" experiments/validation-0996a2cd/historiques.c $core_sources -lm
cc -O2 -std=c11 -Wall -Wextra -Werror -o "$build_dir/garde" experiments/rencontres-ligne/garde.c
cc -O2 -std=c11 -Wall -Wextra -Werror -o "$build_dir/test-garde" experiments/rencontres-ligne/test_garde.c
cc -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Iinclude \
    -o "$build_dir/sonde-phase" experiments/sonde_phase.c src/rng.c src/isa.c -lm
"$build_dir/terrarium" selftest > "$proof_dir/selftest.txt"
"$build_dir/diagnostic" > "$proof_dir/diagnostic.txt"
"$build_dir/historiques" > "$proof_dir/historiques.txt"
# --proc est toujours le répertoire de fixture mkdtemp, jamais le vrai /proc.
"$build_dir/test-garde" "$build_dir/garde" > "$proof_dir/garde-fixtures.txt"
"$build_dir/sonde-phase" selftest > "$proof_dir/sonde-selftest.txt"
"$build_dir/sonde-phase" run "$proof_dir/sonde-run.txt" > "$proof_dir/sonde-run-resume.txt"
"$build_dir/sonde-phase" barre "$proof_dir/sonde-barre.txt" > "$proof_dir/sonde-barre-resume.txt"
for name in vie-monde vie-forces vie-causes vie-forces-briques vie-forces-societes \
    vie-attention vie-traits vie-depense vie-causales vie-saison vie-tronc vie-lecture \
    vie-rencontres-b129054c vie-rencontres-sonnet-41ab85e; do
    if "$build_dir/terrarium" run --dir "$proof_dir/$name" --ticks 1 > "$proof_dir/refus-$name.txt" 2>&1; then
        echo "ECHEC protection $name" >&2; exit 1
    fi
    test ! -e "$proof_dir/$name"
    printf '%s : refus avant création OK\n' "$name" >> "$proof_dir/protections.txt"
done
for name in vie-rencontres-b129054c vie-rencontres-sonnet-41ab85e; do
    mkdir "$proof_dir/$name"
    ln -s "$proof_dir/$name" "$proof_dir/alias-$name"
    if "$build_dir/terrarium" run --dir "$proof_dir/alias-$name/" --ticks 1 > "$proof_dir/refus-alias-$name.txt" 2>&1; then
        echo "ECHEC protection alias $name" >&2; exit 1
    fi
    test -z "$(ls -A "$proof_dir/$name")"
    printf '%s : alias et slash refusés OK\n' "$name" >> "$proof_dir/protections.txt"
done
sha256sum "$build_dir"/* "$proof_dir"/*.txt > "$proof_dir/SHA256SUMS"
git diff --check
cat "$proof_dir/historiques.txt" "$proof_dir/sonde-run-resume.txt" "$proof_dir/sonde-barre-resume.txt"
printf 'Validation OK. Binaries : %s ; preuves : %s\n' "$build_dir" "$proof_dir"
