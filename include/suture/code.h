#ifndef SUTURE_CODE_H
#define SUTURE_CODE_H

#include "disasm.h"
#include "error.h"
#include "stream.h"
#include "types.h"

enum su_frame_item_type {
  SU_ITEM_TOP = 0,
  SU_ITEM_INTEGER = 1,
  SU_ITEM_FLOAT = 2,
  SU_ITEM_DOUBLE = 3,
  SU_ITEM_LONG = 4,
  SU_ITEM_NULL = 5,
  SU_ITEM_UNINITIALIZED_THIS = 6,
  SU_ITEM_OBJECT = 7,
  SU_ITEM_UNINITIALIZED = 8,
};

struct su_frame_item {
  enum su_frame_item_type type;

  union {
    u2 constant;
    struct su_insn *insn;
  };
};

enum su_frame_type {
  SU_FRAME_SAME,
  SU_FRAME_SAME_LOCALS_1_STACK_ITEM,
  SU_FRAME_CHOP,
  SU_FRAME_APPEND,
  SU_FRAME_FULL,
};

struct su_frame {
  enum su_frame_type type;

  u1 chop;

  struct su_frame_item *locals;
  u2 locals_count;

  struct su_frame_item *stack;
  u2 stack_count;
};

struct su_exception {
  struct su_insn *start;
  struct su_insn *end;
  struct su_insn *handler;
  u2 catch_type;
};

struct su_line {
  struct su_insn *start;
  u2 line;
};

struct su_local {
  struct su_insn *start;
  struct su_insn *end;

  u2 name;
  u2 descriptor;
  u2 index;
};

struct su_code_attribute {
  u2 name;

  u1 *bytes;
  u2 length;
};

struct su_code {
  u2 name;

  u2 max_stack;
  u2 max_locals;

  struct su_insn *instructions;

  struct su_exception *exceptions;
  u2 exceptions_count;

  struct su_line *lines;
  u2 lines_count;
  u2 lines_name;

  struct su_local *locals;
  u2 locals_count;
  u2 locals_name;

  struct su_local *local_types;
  u2 local_types_count;
  u2 local_types_name;

  u2 frames_name;

  struct su_code_attribute *attributes;
  u2 attributes_count;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_code_parse(struct su_code *code, void **constant_pool, u2 constant_pool_count, struct su_stream *stream);

enum su_error su_code_build(const struct su_code *code, struct su_stream *stream);

enum su_error su_code_frame(struct su_insn *insn, enum su_frame_type type, u2 locals_count, u2 stack_count);

void su_code_frame_dispose(struct su_insn *insn);

void su_code_dispose(struct su_code *code);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_CODE_H
