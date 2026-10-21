/* Jeu d'instructions Octet-ADN : 1 octet = 1 codon, certains prennent un imm8.
 * 00..0F : ISA historique (compat life.py). 10..1F : extension. Le reste = introns. */
#include <stdio.h>
#include "octet.h"

static const char *NAMES[OP_COUNT] = {
    "NOP", "INC_A", "DEC_A", "INC_B", "DEC_B", "CLR_A", "STORE", "LOAD",
    "JMP", "JZ", "SENSE", "TURN", "STEP", "EAT", "SPLIT", "YIELD",
    "SENSE_NB", "SENSE_EN", "SENSE_STIM", "NEURAL", "EMIT", "MOV_BA", "MOV_AB", "ADD",
    "SUB", "XOR", "ATTACK", "JNZ", "LOADI", "SENSE_HIST", "LOOP", "AND",
};
int op_valid(uint8_t op) { return op < OP_COUNT; }
int op_has_imm(uint8_t op) {
    switch (op) {
    case OP_STORE: case OP_LOAD: case OP_JMP: case OP_JZ: case OP_NEURAL:
    case OP_JNZ: case OP_LOADI: case OP_SENSE_HIST: case OP_AND: return 1;
    default: return 0;
    }
}
const char *op_name(uint8_t op) { return op_valid(op) ? NAMES[op] : "INTRON"; }

size_t disasm(const uint8_t *dna, size_t len, int skip_introns, char *out, size_t cap) {
    size_t o = 0, i = 0;
    if (cap) out[0] = 0;
    while (i < len && o + 40 < cap) {
        uint8_t op = dna[i];
        int imm = (op_has_imm(op) && i + 1 < len) ? dna[i + 1] : -1;
        if (!(skip_introns && (op == OP_NOP || !op_valid(op)))) {
            if (!op_valid(op)) o += snprintf(out + o, cap - o, "%04zx  INTRON:%02x\n", i, op);
            else if (imm >= 0) o += snprintf(out + o, cap - o, "%04zx  %02x %02x  %s %d\n", i, op, imm, NAMES[op], imm);
            else o += snprintf(out + o, cap - o, "%04zx  %02x     %s\n", i, op, NAMES[op]);
        }
        i += imm >= 0 ? 2 : 1;
    }
    return o;
}
