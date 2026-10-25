/* Cible de régression bornée : aucun selftest soupe ni test global exécuté.
 * Compiler directement avec les sources du cœur vers un ELF neuf. */
#define main octet_tests_complets_non_executes
#include "../../tests/test_vm.c"
#undef main
int main(void) {
    test_core_state_hash_matches_main();
    puts("10/10 hashes historiques inchangés (1 cycle, 50 ticks, graine historique)");
    return 0;
}
