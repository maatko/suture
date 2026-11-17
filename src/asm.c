#include <suture/asm.h>
#include <suture/opcodes.h>

#include "internal.h"

static bool su_asm_type(const u1 opcode, enum su_insn_type *type) {
  if ((opcode >= ILOAD_0 && opcode <= ALOAD_3) || (opcode >= ISTORE_0 && opcode <= ASTORE_3) || opcode == LDC_W || opcode == WIDE || opcode > IFNONNULL)
    return false;

  switch (opcode) {
    case BIPUSH:
    case SIPUSH:
    case NEWARRAY:
      *type = SU_INSN_VAR_INT;
      break;
    case LDC:
    case LDC2_W:
      *type = SU_INSN_VAR_LDC;
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
      *type = SU_INSN_VAR;
      break;
    case IINC:
      *type = SU_INSN_IINC;
      break;
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
      *type = SU_INSN_JUMP;
      break;
    case TABLESWITCH:
    case LOOKUPSWITCH:
      *type = SU_INSN_SWITCH;
      break;
    case GETSTATIC:
    case PUTSTATIC:
    case GETFIELD:
    case PUTFIELD:
      *type = SU_INSN_FIELD;
      break;
    case INVOKEVIRTUAL:
    case INVOKESPECIAL:
    case INVOKESTATIC:
    case INVOKEINTERFACE:
      *type = SU_INSN_METHOD;
      break;
    case INVOKEDYNAMIC:
      *type = SU_INSN_INVOKE_DYNAMIC;
      break;
    case NEW:
    case ANEWARRAY:
    case CHECKCAST:
    case INSTANCEOF:
      *type = SU_INSN_TYPE;
      break;
    case MULTIANEWARRAY:
      *type = SU_INSN_MULTI_ANEW_ARRAY;
      break;
    default:
      *type = SU_INSN;
      break;
  }

  return true;
}

static enum su_error su_asm_check(const struct su_insn *insn) {
  enum su_insn_type type;
  if (!su_asm_type(insn->opcode, &type))
    return SU_CLASS_CODE_UNRECOGNIZED_INSTRUCTION;

  if (insn->type != type)
    return SU_CLASS_CODE_INVALID_OPERAND;

  switch (insn->type) {
    case SU_INSN_VAR_INT:
      if (insn->opcode == BIPUSH && (insn->operand < INT8_MIN || insn->operand > INT8_MAX))
        return SU_CLASS_CODE_INVALID_OPERAND;

      if (insn->opcode == SIPUSH && (insn->operand < INT16_MIN || insn->operand > INT16_MAX))
        return SU_CLASS_CODE_INVALID_OPERAND;

      if (insn->opcode == NEWARRAY && (insn->operand < 0 || insn->operand > UINT8_MAX))
        return SU_CLASS_CODE_INVALID_OPERAND;
      break;
    case SU_INSN_JUMP:
      if (insn->target == NULL)
        return SU_CLASS_CODE_INVALID_JUMP;
      break;
    case SU_INSN_SWITCH: {
      const struct su_insn_switch *table = &insn->table;
      if (table->fallback == NULL)
        return SU_CLASS_CODE_INVALID_JUMP;

      if (table->count > UINT16_MAX / sizeof(u4) || (table->count == 0 && insn->opcode == TABLESWITCH))
        return SU_CLASS_CODE_INVALID_SWITCH;
      if (table->count > 0 && (table->keys == NULL || table->targets == NULL))
        return SU_CLASS_CODE_INVALID_SWITCH;

      for (u4 i = 0; i < table->count; i++) {
        if (table->targets[i] == NULL)
          return SU_CLASS_CODE_INVALID_JUMP;

        if (insn->opcode == TABLESWITCH && i > 0 && (s8)table->keys[i] != (s8)table->keys[i - 1] + 1)
          return SU_CLASS_CODE_INVALID_SWITCH;
      }
    } break;
    default:
      break;
  }

  return SU_OK;
}

static bool su_asm_iinc_wide(const struct su_insn *insn) {
  return insn->iinc.var > UINT8_MAX || insn->iinc.increment < INT8_MIN || insn->iinc.increment > INT8_MAX;
}

static u4 su_asm_size(const struct su_insn *insn, const u4 pc, const bool wide) {
  switch (insn->type) {
    case SU_INSN:
      return 1;
    case SU_INSN_VAR:
      if (insn->opcode != RET && insn->var <= 3)
        return 1;

      return insn->var <= UINT8_MAX ? 2 : 4;
    case SU_INSN_VAR_INT:
      return insn->opcode == SIPUSH ? 3 : 2;
    case SU_INSN_VAR_LDC:
      return insn->opcode == LDC && insn->constant <= UINT8_MAX ? 2 : 3;
    case SU_INSN_JUMP:
      return wide ? 5 : 3;
    case SU_INSN_SWITCH:
      if (insn->opcode == TABLESWITCH)
        return 1 + (3 - pc % 4) + 12 + 4 * insn->table.count;

      return 1 + (3 - pc % 4) + 8 + 8 * insn->table.count;
    case SU_INSN_METHOD:
      return insn->opcode == INVOKEINTERFACE ? 5 : 3;
    case SU_INSN_FIELD:
    case SU_INSN_TYPE:
      return 3;
    case SU_INSN_INVOKE_DYNAMIC:
      return 5;
    case SU_INSN_IINC:
      return su_asm_iinc_wide(insn) ? 6 : 3;
    case SU_INSN_MULTI_ANEW_ARRAY:
      return 4;
  }

  return 0;
}

static enum su_error su_asm_layout(struct su_insn *instructions, const bool *wide, u4 *code_length) {
  u4 pc = 0;
  u4 index = 0;

  for (struct su_insn *insn = instructions; insn != NULL; insn = insn->next) {
    insn->pc = (u2)pc;
    pc += su_asm_size(insn, pc, wide[index++]);

    if (pc > UINT16_MAX)
      return SU_CLASS_CODE_TOO_LARGE;
  }

  *code_length = pc;
  return SU_OK;
}

static bool su_asm_contains(struct su_insn **labels, const u4 code_length, const struct su_insn *insn) {
  return insn->pc < code_length && labels[insn->pc] == insn;
}

static enum su_error su_asm_write(struct su_stream *stream, const struct su_insn *insn, const bool wide) {
  enum su_error status = SU_OK;

  switch (insn->type) {
    case SU_INSN:
      SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
      break;
    case SU_INSN_VAR:
      if (insn->opcode != RET && insn->var <= 3) {
        const u1 folded = insn->opcode < ISTORE ? ILOAD_0 + (insn->opcode - ILOAD) * 4 : ISTORE_0 + (insn->opcode - ISTORE) * 4;
        SU_TRY(status, su_stream_w1(stream, (u1)(folded + insn->var), 0));
      } else if (insn->var <= UINT8_MAX) {
        SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
        SU_TRY(status, su_stream_w1(stream, (u1)insn->var, 0));
      } else {
        SU_TRY(status, su_stream_w1(stream, WIDE, 0));
        SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
        SU_TRY(status, su_stream_w2(stream, insn->var, 0));
      }
      break;
    case SU_INSN_VAR_INT:
      SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
      if (insn->opcode == SIPUSH) {
        SU_TRY(status, su_stream_w2(stream, (u2)insn->operand, 0));
      } else {
        SU_TRY(status, su_stream_w1(stream, (u1)insn->operand, 0));
      }
      break;
    case SU_INSN_VAR_LDC:
      if (insn->opcode == LDC && insn->constant <= UINT8_MAX) {
        SU_TRY(status, su_stream_w1(stream, LDC, 0));
        SU_TRY(status, su_stream_w1(stream, (u1)insn->constant, 0));
      } else {
        SU_TRY(status, su_stream_w1(stream, insn->opcode == LDC ? LDC_W : LDC2_W, 0));
        SU_TRY(status, su_stream_w2(stream, insn->constant, 0));
      }
      break;
    case SU_INSN_JUMP: {
      const s4 offset = (s4)insn->target->pc - (s4)insn->pc;
      if (wide) {
        SU_TRY(status, su_stream_w1(stream, insn->opcode == GOTO ? GOTO_W : JSR_W, 0));
        SU_TRY(status, su_stream_w4(stream, (u4)offset, 0));
      } else {
        SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
        SU_TRY(status, su_stream_w2(stream, (u2)offset, 0));
      }
    } break;
    case SU_INSN_SWITCH: {
      const struct su_insn_switch *table = &insn->table;
      SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));

      for (u2 i = 0; i < 3 - insn->pc % 4; i++) {
        SU_TRY(status, su_stream_w1(stream, 0, 0));
      }

      SU_TRY(status, su_stream_w4(stream, (u4)((s4)table->fallback->pc - (s4)insn->pc), 0));

      if (insn->opcode == TABLESWITCH) {
        SU_TRY(status, su_stream_w4(stream, (u4)table->keys[0], 0));
        SU_TRY(status, su_stream_w4(stream, (u4)table->keys[table->count - 1], 0));
      } else {
        SU_TRY(status, su_stream_w4(stream, table->count, 0));
      }

      for (u4 i = 0; i < table->count; i++) {
        if (insn->opcode == LOOKUPSWITCH) {
          SU_TRY(status, su_stream_w4(stream, (u4)table->keys[i], 0));
        }
        SU_TRY(status, su_stream_w4(stream, (u4)((s4)table->targets[i]->pc - (s4)insn->pc), 0));
      }
    } break;
    case SU_INSN_METHOD:
      SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
      SU_TRY(status, su_stream_w2(stream, insn->method.constant, 0));

      if (insn->opcode == INVOKEINTERFACE) {
        SU_TRY(status, su_stream_w1(stream, insn->method.count, 0));
        SU_TRY(status, su_stream_w1(stream, 0, 0));
      }
      break;
    case SU_INSN_FIELD:
    case SU_INSN_TYPE:
      SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
      SU_TRY(status, su_stream_w2(stream, insn->constant, 0));
      break;
    case SU_INSN_INVOKE_DYNAMIC:
      SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
      SU_TRY(status, su_stream_w2(stream, insn->constant, 0));
      SU_TRY(status, su_stream_w2(stream, 0, 0));
      break;
    case SU_INSN_IINC:
      if (su_asm_iinc_wide(insn)) {
        SU_TRY(status, su_stream_w1(stream, WIDE, 0));
        SU_TRY(status, su_stream_w1(stream, IINC, 0));
        SU_TRY(status, su_stream_w2(stream, insn->iinc.var, 0));
        SU_TRY(status, su_stream_w2(stream, (u2)insn->iinc.increment, 0));
      } else {
        SU_TRY(status, su_stream_w1(stream, IINC, 0));
        SU_TRY(status, su_stream_w1(stream, (u1)insn->iinc.var, 0));
        SU_TRY(status, su_stream_w1(stream, (u1)insn->iinc.increment, 0));
      }
      break;
    case SU_INSN_MULTI_ANEW_ARRAY:
      SU_TRY(status, su_stream_w1(stream, insn->opcode, 0));
      SU_TRY(status, su_stream_w2(stream, insn->multi_anew_array.constant, 0));
      SU_TRY(status, su_stream_w1(stream, insn->multi_anew_array.dimensions, 0));
      break;
  }

  return status;
}

enum su_error su_asm_insn(struct su_insn **insn, const u1 opcode) {
  if (insn == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  u1 folded = opcode;
  u2 var = 0;

  if (opcode >= ILOAD_0 && opcode <= ALOAD_3) {
    folded = ILOAD + (opcode - ILOAD_0) / 4;
    var = (opcode - ILOAD_0) % 4;
  } else if (opcode >= ISTORE_0 && opcode <= ASTORE_3) {
    folded = ISTORE + (opcode - ISTORE_0) / 4;
    var = (opcode - ISTORE_0) % 4;
  } else if (opcode == LDC_W) {
    folded = LDC;
  } else if (opcode == GOTO_W) {
    folded = GOTO;
  } else if (opcode == JSR_W) {
    folded = JSR;
  }

  enum su_insn_type type;
  if (!su_asm_type(folded, &type))
    return SU_CLASS_CODE_UNRECOGNIZED_INSTRUCTION;

  struct su_insn *created = calloc(1, sizeof(struct su_insn));
  if (created == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  created->type = type;
  created->opcode = folded;

  if (type == SU_INSN_VAR)
    created->var = var;

  *insn = created;
  return SU_OK;
}

enum su_error su_asm_insert_before(struct su_insn **instructions, struct su_insn *position, struct su_insn *insn) {
  if (instructions == NULL || insn == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (position == NULL) {
    struct su_insn *last = (*instructions);
    while (last != NULL && last->next != NULL)
      last = last->next;

    return su_asm_insert_after(instructions, last, insn);
  }

  insn->prev = position->prev;
  insn->next = position;

  if (position->prev != NULL)
    position->prev->next = insn;
  else
    *instructions = insn;

  position->prev = insn;
  return SU_OK;
}

enum su_error su_asm_insert_after(struct su_insn **instructions, struct su_insn *position, struct su_insn *insn) {
  if (instructions == NULL || insn == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (position == NULL) {
    insn->prev = NULL;
    insn->next = (*instructions);

    if (insn->next != NULL)
      insn->next->prev = insn;

    *instructions = insn;
    return SU_OK;
  }

  insn->prev = position;
  insn->next = position->next;

  if (position->next != NULL)
    position->next->prev = insn;

  position->next = insn;
  return SU_OK;
}

enum su_error su_asm_build(struct su_insn *instructions, struct su_stream *stream, u4 *code_length) {
  if (stream == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (instructions == NULL)
    return SU_CLASS_CODE_MISSING;

  enum su_error status = SU_OK;

  u4 count = 0;
  for (const struct su_insn *insn = instructions; insn != NULL; insn = insn->next) {
    SU_TRY(status, su_asm_check(insn));
    count++;
  }

  struct su_stream code = { 0 };
  struct su_insn **labels = NULL;

  bool *wide = calloc(count, sizeof(bool));
  if (wide == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  u4 length = 0;
  SU_TRY_CATCH(status, su_asm_layout(instructions, wide, &length), exit);

  labels = calloc(length, sizeof(struct su_insn *));
  if (labels == NULL) {
    status = SU_MEMORY_ALLOCATION_FAILURE;
    goto exit;
  }

  for (struct su_insn *insn = instructions; insn != NULL; insn = insn->next)
    labels[insn->pc] = insn;

  for (const struct su_insn *insn = instructions; insn != NULL; insn = insn->next) {
    bool contained = true;
    if (insn->type == SU_INSN_JUMP) {
      contained = su_asm_contains(labels, length, insn->target);
    } else if (insn->type == SU_INSN_SWITCH) {
      contained = su_asm_contains(labels, length, insn->table.fallback);
      for (u4 i = 0; i < insn->table.count && contained; i++)
        contained = su_asm_contains(labels, length, insn->table.targets[i]);
    }

    if (!contained) {
      status = SU_CLASS_CODE_INVALID_JUMP;
      goto exit;
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;

    u4 index = 0;
    for (const struct su_insn *insn = instructions; insn != NULL; insn = insn->next, index++) {
      if (insn->type != SU_INSN_JUMP || wide[index] || (insn->opcode != GOTO && insn->opcode != JSR))
        continue;

      const s4 offset = (s4)insn->target->pc - (s4)insn->pc;
      if (offset < INT16_MIN || offset > INT16_MAX) {
        wide[index] = true;
        changed = true;
      }
    }

    if (changed) {
      SU_TRY_CATCH(status, su_asm_layout(instructions, wide, &length), exit);
    }
  }

  code.buffer = malloc(length);
  if (code.buffer == NULL) {
    status = SU_MEMORY_ALLOCATION_FAILURE;
    goto exit;
  }

  code.length = (u2)length;

  u4 index = 0;
  for (const struct su_insn *insn = instructions; insn != NULL; insn = insn->next, index++) {
    const s4 offset = insn->type == SU_INSN_JUMP ? (s4)insn->target->pc - (s4)insn->pc : 0;
    if (!wide[index] && (offset < INT16_MIN || offset > INT16_MAX)) {
      status = SU_CLASS_CODE_INVALID_JUMP;
      goto exit;
    }

    SU_TRY_CATCH(status, su_asm_write(&code, insn, wide[index]), exit);
  }

  SU_TRY_CATCH(status, su_stream_wn(stream, code.buffer, code.cursor, 0), exit);

  if (code_length != NULL)
    *code_length = length;

exit:
  SU_FREE(code.buffer);
  SU_FREE(labels);
  SU_FREE(wide);
  return status;
}
