#include <suture/disasm.h>

enum su_error su_disasm_parse(struct su_insn **instructions, struct su_stream *code) {
  code->cursor = 0;
  return SU_OK;
}