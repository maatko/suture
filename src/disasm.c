#include <suture/disasm.h>
#include <suture/opcodes.h>

#include "internal.h"

enum su_error su_disasm_parse(struct su_insn **instructions, void **constant_pool, u2 constant_pool_count, struct su_stream *code, const u4 code_length) {
  enum su_error status = SU_OK;
  if (code_length >= 65536)
    return SU_CLASS_CODE_TOO_LARGE;

  u2 pc = 0;
  while (pc < code_length) {
    u1 instruction;
    SU_TRY(status, su_stream_r1(code, &instruction, 0));

    switch (instruction) {
      case 0x2A:
        break;
      case 0xBc:
        break;
      default:
        printf("Failed to pares instruction %#X\n", instruction);
        return SU_CLASS_CODE_UNRECOGNIZED_INSTRUCTION;
    }
    pc++;
  }

  return status;
}
