#ifndef SUTURE_DISASM_H
#define SUTURE_DISASM_H

#include "error.h"
#include "stream.h"
#include "types.h"

enum su_insn_type {
  SU_INSN,
  SU_INSN_VAR,
  SU_INSN_VAR_INT,
  SU_INSN_VAR_LDC,
  SU_INSN_JUMP,
  SU_INSN_SWITCH,
  SU_INSN_METHOD,
  SU_INSN_FIELD,
  SU_INSN_INVOKE_DYNAMIC,
  SU_INSN_TYPE,
  SU_INSN_IINC,
  SU_INSN_MULTI_ANEW_ARRAY,
};

struct su_insn {
  enum su_insn_type type;

  u1 opcode;
  union {
    u2 var;
  } parameter;

  struct su_insn *next;
  struct su_insn *prev;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_disasm_parse(struct su_insn **instructions, void **constant_pool, u2 constant_pool_count, struct su_stream *code, u4 code_length);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_DISASM_H
