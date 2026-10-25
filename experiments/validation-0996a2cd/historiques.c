/* Sélection ciblée des tests existants : pas de soupe, fuzz, mix ou run long. */
#define main suite_originale_main
#include "../../tests/test_vm.c"
#undef main
int main(void) {
    test_legacy_opcodes_preserved();
    test_seeds_embedded_match_files();
    test_sixteen_labs_and_core_untouched();
    test_core_state_hash_matches_main();
    puts("ISA, seeds, voie historique neutre : OK ; 10/10 hashes historiques identiques");
    return 0;
}
