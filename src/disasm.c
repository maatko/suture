#include <suture/code.h>
#include <suture/disasm.h>
#include <suture/opcodes.h>

#include "internal.h"

static void su_disasm_free(struct su_insn *insn) {
  if (insn == NULL)
    return;

  if (insn->type == SU_INSN_SWITCH) {
    SU_FREE(insn->table.keys);
    SU_FREE(insn->table.targets);
  }

  su_code_frame_dispose(insn);
  SU_FREE(insn);
}

static enum su_error su_disasm_label(struct su_insn **labels, const u4 code_length, const s8 pc, struct su_insn **label) {
  if (pc < 0 || pc >= code_length)
    return SU_CLASS_CODE_INVALID_JUMP;

  if (labels[pc] == NULL) {
    labels[pc] = calloc(1, sizeof(struct su_insn));
    if (labels[pc] == NULL)
      return SU_MEMORY_ALLOCATION_FAILURE;
  }

  *label = labels[pc];
  return SU_OK;
}

static enum su_error su_disasm_index(struct su_stream *stream, const bool wide, u2 *index) {
  if (wide)
    return su_stream_r2(stream, index, 0);

  enum su_error status = SU_OK;

  u1 narrow;
  SU_TRY(status, su_stream_r1(stream, &narrow, 0));

  *index = narrow;
  return status;
}

static enum su_error su_disasm_constant(struct su_stream *stream, const u2 constant_pool_count, const bool wide, u2 *index) {
  enum su_error status = SU_OK;
  SU_TRY(status, su_disasm_index(stream, wide, index));

  if (*index == 0 || *index >= constant_pool_count)
    return SU_CLASS_CODE_INVALID_CONSTANT;

  return status;
}

static enum su_error su_disasm_jump(struct su_stream *stream, struct su_insn **labels, const u2 pc, const bool wide, struct su_insn **target) {
  enum su_error status = SU_OK;

  s4 offset;
  if (wide) {
    u4 value;
    SU_TRY(status, su_stream_r4(stream, &value, 0));

    offset = (s4)value;
  } else {
    u2 value;
    SU_TRY(status, su_stream_r2(stream, &value, 0));

    offset = (s2)value;
  }

  return su_disasm_label(labels, stream->length, (s8)pc + offset, target);
}

static enum su_error su_disasm_switch(struct su_stream *stream, struct su_insn **labels, struct su_insn *insn, const u2 pc) {
  enum su_error status = SU_OK;
  struct su_insn_switch *table = &insn->table;

  const u2 padding = 3 - pc % 4;

  u4 fallback;
  SU_TRY(status, su_stream_r4(stream, &fallback, padding));
  SU_TRY(status, su_disasm_label(labels, stream->length, (s8)pc + (s4)fallback, &table->fallback));

  u4 low = 0;
  u4 count;

  if (insn->opcode == TABLESWITCH) {
    u4 high;
    SU_TRY(status, su_stream_r4(stream, &low, 0));
    SU_TRY(status, su_stream_r4(stream, &high, 0));

    if ((s4)low > (s4)high)
      return SU_CLASS_CODE_INVALID_SWITCH;

    if (high - low >= (u4)(stream->length - stream->cursor) / sizeof(u4))
      return SU_STREAM_AT_END;

    count = high - low + 1;
  } else {
    SU_TRY(status, su_stream_r4(stream, &count, 0));

    if (count > (u4)(stream->length - stream->cursor) / (sizeof(u4) * 2))
      return SU_STREAM_AT_END;
  }

  if (count == 0)
    return status;

  table->keys = calloc(count, sizeof(s4));
  table->targets = calloc(count, sizeof(struct su_insn *));
  if (table->keys == NULL || table->targets == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  table->count = count;

  for (u4 i = 0; i < count; i++) {
    u4 key = low + i;
    if (insn->opcode == LOOKUPSWITCH) {
      SU_TRY(status, su_stream_r4(stream, &key, 0));
    }

    u4 offset;
    SU_TRY(status, su_stream_r4(stream, &offset, 0));

    table->keys[i] = (s4)key;
    SU_TRY(status, su_disasm_label(labels, stream->length, (s8)pc + (s4)offset, &table->targets[i]));
  }

  return status;
}

enum su_error su_disasm_parse(struct su_insn **instructions, void **constant_pool, const u2 constant_pool_count, struct su_stream *code, const u4 code_length) {
  if (instructions == NULL || code == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (code_length == 0)
    return SU_CLASS_CODE_MISSING;

  if (code_length >= 65536)
    return SU_CLASS_CODE_TOO_LARGE;

  if (code_length > (u4)(code->length - code->cursor))
    return SU_STREAM_AT_END;

  enum su_error status = SU_OK;
  struct su_stream stream = {
    .buffer = code->buffer + code->cursor,
    .length = code_length,
    .cursor = 0,
    .chunk = 0
  };

  struct su_insn **labels = calloc(code_length, sizeof(struct su_insn *));
  if (labels == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  struct su_insn *head = NULL;
  struct su_insn *tail = NULL;

  while (stream.cursor < stream.length) {
    const u2 pc = (u2)stream.cursor;

    u1 opcode;
    SU_TRY_CATCH(status, su_stream_r1(&stream, &opcode, 0), exit);

    const bool wide = opcode == WIDE;
    if (wide) {
      SU_TRY_CATCH(status, su_stream_r1(&stream, &opcode, 0), exit);

      if ((opcode < ILOAD || opcode > ALOAD) && (opcode < ISTORE || opcode > ASTORE) && opcode != RET && opcode != IINC) {
        status = SU_CLASS_CODE_UNRECOGNIZED_INSTRUCTION;
        goto exit;
      }
    }

    struct su_insn *insn = NULL;
    SU_TRY_CATCH(status, su_disasm_label(labels, stream.length, pc, &insn), exit);

    insn->opcode = opcode;
    insn->pc = pc;

    switch (opcode) {
      case BIPUSH:
      case NEWARRAY: {
        u1 value;
        SU_TRY_CATCH(status, su_stream_r1(&stream, &value, 0), exit);

        insn->type = SU_INSN_VAR_INT;
        insn->operand = opcode == BIPUSH ? (s1)value : value;
      } break;
      case SIPUSH: {
        u2 value;
        SU_TRY_CATCH(status, su_stream_r2(&stream, &value, 0), exit);

        insn->type = SU_INSN_VAR_INT;
        insn->operand = (s2)value;
      } break;
      case LDC:
      case LDC_W:
      case LDC2_W:
        insn->type = SU_INSN_VAR_LDC;
        SU_TRY_CATCH(status, su_disasm_constant(&stream, constant_pool_count, opcode != LDC, &insn->constant), exit);

        // ldc2_w is kept, it is the only one that loads a long or a double
        if (opcode == LDC_W)
          insn->opcode = LDC;
        break;
      case ILOAD:
      case LLOAD:
      case FLOAD:
      case DLOAD:
      case ALOAD:
      case ISTORE:
      case LSTORE:
      case FSTORE:
      case DSTORE:
      case ASTORE:
      case RET:
        insn->type = SU_INSN_VAR;
        SU_TRY_CATCH(status, su_disasm_index(&stream, wide, &insn->var), exit);
        break;
      case ILOAD_0:
      case ILOAD_1:
      case ILOAD_2:
      case ILOAD_3:
      case LLOAD_0:
      case LLOAD_1:
      case LLOAD_2:
      case LLOAD_3:
      case FLOAD_0:
      case FLOAD_1:
      case FLOAD_2:
      case FLOAD_3:
      case DLOAD_0:
      case DLOAD_1:
      case DLOAD_2:
      case DLOAD_3:
      case ALOAD_0:
      case ALOAD_1:
      case ALOAD_2:
      case ALOAD_3:
        insn->type = SU_INSN_VAR;
        insn->opcode = ILOAD + (opcode - ILOAD_0) / 4;
        insn->var = (opcode - ILOAD_0) % 4;
        break;
      case ISTORE_0:
      case ISTORE_1:
      case ISTORE_2:
      case ISTORE_3:
      case LSTORE_0:
      case LSTORE_1:
      case LSTORE_2:
      case LSTORE_3:
      case FSTORE_0:
      case FSTORE_1:
      case FSTORE_2:
      case FSTORE_3:
      case DSTORE_0:
      case DSTORE_1:
      case DSTORE_2:
      case DSTORE_3:
      case ASTORE_0:
      case ASTORE_1:
      case ASTORE_2:
      case ASTORE_3:
        insn->type = SU_INSN_VAR;
        insn->opcode = ISTORE + (opcode - ISTORE_0) / 4;
        insn->var = (opcode - ISTORE_0) % 4;
        break;
      case IINC: {
        u2 increment;
        SU_TRY_CATCH(status, su_disasm_index(&stream, wide, &insn->iinc.var), exit);
        SU_TRY_CATCH(status, su_disasm_index(&stream, wide, &increment), exit);

        insn->type = SU_INSN_IINC;
        insn->iinc.increment = wide ? (s2)increment : (s1)increment;
      } break;
      case IFEQ:
      case IFNE:
      case IFLT:
      case IFGE:
      case IFGT:
      case IFLE:
      case IF_ICMPEQ:
      case IF_ICMPNE:
      case IF_ICMPLT:
      case IF_ICMPGE:
      case IF_ICMPGT:
      case IF_ICMPLE:
      case IF_ACMPEQ:
      case IF_ACMPNE:
      case GOTO:
      case JSR:
      case IFNULL:
      case IFNONNULL:
        insn->type = SU_INSN_JUMP;
        SU_TRY_CATCH(status, su_disasm_jump(&stream, labels, pc, false, &insn->target), exit);
        break;
      case GOTO_W:
      case JSR_W:
        insn->type = SU_INSN_JUMP;
        insn->opcode = opcode == GOTO_W ? GOTO : JSR;
        SU_TRY_CATCH(status, su_disasm_jump(&stream, labels, pc, true, &insn->target), exit);
        break;
      case TABLESWITCH:
      case LOOKUPSWITCH:
        insn->type = SU_INSN_SWITCH;
        SU_TRY_CATCH(status, su_disasm_switch(&stream, labels, insn, pc), exit);
        break;
      case GETSTATIC:
      case PUTSTATIC:
      case GETFIELD:
      case PUTFIELD:
        insn->type = SU_INSN_FIELD;
        SU_TRY_CATCH(status, su_disasm_constant(&stream, constant_pool_count, true, &insn->constant), exit);
        break;
      case INVOKEVIRTUAL:
      case INVOKESPECIAL:
      case INVOKESTATIC:
        insn->type = SU_INSN_METHOD;
        SU_TRY_CATCH(status, su_disasm_constant(&stream, constant_pool_count, true, &insn->method.constant), exit);
        break;
      case INVOKEINTERFACE: {
        insn->type = SU_INSN_METHOD;
        SU_TRY_CATCH(status, su_disasm_constant(&stream, constant_pool_count, true, &insn->method.constant), exit);
        SU_TRY_CATCH(status, su_stream_r1(&stream, &insn->method.count, 0), exit);

        u1 reserved;
        SU_TRY_CATCH(status, su_stream_r1(&stream, &reserved, 0), exit);
      } break;
      case INVOKEDYNAMIC: {
        insn->type = SU_INSN_INVOKE_DYNAMIC;
        SU_TRY_CATCH(status, su_disasm_constant(&stream, constant_pool_count, true, &insn->constant), exit);

        u2 reserved;
        SU_TRY_CATCH(status, su_stream_r2(&stream, &reserved, 0), exit);
      } break;
      case NEW:
      case ANEWARRAY:
      case CHECKCAST:
      case INSTANCEOF:
        insn->type = SU_INSN_TYPE;
        SU_TRY_CATCH(status, su_disasm_constant(&stream, constant_pool_count, true, &insn->constant), exit);
        break;
      case MULTIANEWARRAY:
        insn->type = SU_INSN_MULTI_ANEW_ARRAY;
        SU_TRY_CATCH(status, su_disasm_constant(&stream, constant_pool_count, true, &insn->multi_anew_array.constant), exit);
        SU_TRY_CATCH(status, su_stream_r1(&stream, &insn->multi_anew_array.dimensions, 0), exit);
        break;
      default:
        if (opcode > JSR_W) {
          status = SU_CLASS_CODE_UNRECOGNIZED_INSTRUCTION;
          goto exit;
        }

        insn->type = SU_INSN;
        break;
    }

    insn->prev = tail;
    if (tail != NULL)
      tail->next = insn;
    else
      head = insn;

    tail = insn;
  }

  for (u4 i = 0; i < code_length; i++) {
    if (labels[i] != NULL && labels[i] != head && labels[i]->prev == NULL) {
      status = SU_CLASS_CODE_INVALID_JUMP;
      goto exit;
    }
  }

  code->cursor += stream.length;
  *instructions = head;

exit:
  if (status != SU_OK) {
    for (u4 i = 0; i < code_length; i++)
      su_disasm_free(labels[i]);
  }

  SU_FREE(labels);
  return status;
}

void su_disasm_dispose(struct su_insn *instructions) {
  struct su_insn *insn = instructions;
  while (insn != NULL) {
    struct su_insn *next = insn->next;

    su_disasm_free(insn);
    insn = next;
  }
}
