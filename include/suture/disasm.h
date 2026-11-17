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

struct su_insn;
struct su_frame;

struct su_insn_switch {
  struct su_insn *fallback;

  s4 *keys;
  struct su_insn **targets;
  u4 count;
};

struct su_insn_method {
  u2 constant;
  u1 count;
};

struct su_insn_iinc {
  u2 var;
  s2 increment;
};

struct su_insn_multi_anew_array {
  u2 constant;
  u1 dimensions;
};

struct su_insn {
  enum su_insn_type type;

  u1 opcode;
  u2 pc;

  union {
    u2 var;
    s4 operand;
    u2 constant;
    struct su_insn *target;
    struct su_insn_switch table;
    struct su_insn_method method;
    struct su_insn_iinc iinc;
    struct su_insn_multi_anew_array multi_anew_array;
  };

  struct su_frame *frame;

  struct su_insn *next;
  struct su_insn *prev;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_disasm_parse(struct su_insn **instructions, void **constant_pool, u2 constant_pool_count, struct su_stream *code, u4 code_length);

void su_disasm_dispose(struct su_insn *instructions);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_DISASM_H
