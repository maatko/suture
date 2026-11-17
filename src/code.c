#include <suture/asm.h>
#include <suture/code.h>
#include <suture/opcodes.h>

#include "internal.h"

#define SAME_LOCALS_1_STACK_ITEM 64
#define SAME_LOCALS_1_STACK_ITEM_EXTENDED 247
#define SAME_FRAME_EXTENDED 251
#define FULL_FRAME 255

struct su_code_labels {
  struct su_insn **table;
  u4 length;
};

static enum su_error su_code_labels_init(struct su_code_labels *labels, struct su_insn *instructions, const u4 code_length) {
  labels->table = calloc(code_length, sizeof(struct su_insn *));
  if (labels->table == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  labels->length = code_length;

  for (struct su_insn *insn = instructions; insn != NULL; insn = insn->next)
    labels->table[insn->pc] = insn;

  return SU_OK;
}

static struct su_insn *su_code_label(const struct su_code_labels *labels, const u4 pc) {
  return pc < labels->length ? labels->table[pc] : NULL;
}

static bool su_code_pc(const struct su_code_labels *labels, const struct su_insn *insn, const bool end, u2 *pc) {
  if (insn == NULL) {
    *pc = (u2)labels->length;
    return end;
  }

  if (su_code_label(labels, insn->pc) != insn)
    return false;

  *pc = insn->pc;
  return true;
}

static enum su_error su_code_slice(struct su_stream *stream, const u4 length, struct su_stream *slice) {
  if (length > (u4)(stream->length - stream->cursor))
    return SU_STREAM_AT_END;

  *slice = (struct su_stream){
    .buffer = stream->buffer + stream->cursor,
    .length = (u2)length,
    .cursor = 0,
    .chunk = 0
  };

  stream->cursor += (u2)length;
  return SU_OK;
}

static enum su_error su_code_patch(struct su_stream *stream, const u2 position, const u4 value) {
  const u2 cursor = stream->cursor;
  stream->cursor = position;

  const enum su_error status = su_stream_w4(stream, value, 0);

  stream->cursor = cursor;
  return status;
}

static enum su_error su_code_begin(struct su_stream *stream, const u2 name, u2 *attribute) {
  if (name == 0)
    return SU_CLASS_CODE_INVALID_CONSTANT;

  enum su_error status = SU_OK;
  SU_TRY(status, su_stream_w2(stream, name, 0));

  *attribute = stream->cursor;
  return su_stream_w4(stream, 0, 0);
}

static enum su_error su_code_end(struct su_stream *stream, const u2 attribute) {
  return su_code_patch(stream, attribute, (u4)(stream->cursor - attribute - sizeof(u4)));
}

static enum su_error su_code_parse_exceptions(struct su_code *code, struct su_stream *stream, const struct su_code_labels *labels) {
  enum su_error status = SU_OK;

  u2 count;
  SU_TRY(status, su_stream_r2(stream, &count, 0));

  if (count > (stream->length - stream->cursor) / (sizeof(u2) * 4))
    return SU_STREAM_AT_END;

  if (count == 0)
    return status;

  code->exceptions = calloc(count, sizeof(struct su_exception));
  if (code->exceptions == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  code->exceptions_count = count;

  for (u2 i = 0; i < count; i++) {
    struct su_exception *exception = &code->exceptions[i];

    u2 start, end, handler;
    SU_TRY(status, su_stream_r2(stream, &start, 0));
    SU_TRY(status, su_stream_r2(stream, &end, 0));
    SU_TRY(status, su_stream_r2(stream, &handler, 0));
    SU_TRY(status, su_stream_r2(stream, &exception->catch_type, 0));

    exception->start = su_code_label(labels, start);
    exception->end = su_code_label(labels, end);
    exception->handler = su_code_label(labels, handler);

    if (exception->start == NULL || exception->handler == NULL || (exception->end == NULL && end != labels->length) || start >= end)
      return SU_CLASS_CODE_INVALID_EXCEPTION;
  }

  return status;
}

static enum su_error su_code_parse_lines(struct su_code *code, struct su_stream *stream, const struct su_code_labels *labels) {
  enum su_error status = SU_OK;

  u2 count;
  SU_TRY(status, su_stream_r2(stream, &count, 0));

  if (count > (stream->length - stream->cursor) / (sizeof(u2) * 2))
    return SU_STREAM_AT_END;

  if (count == 0)
    return status;

  void *lines = realloc(code->lines, sizeof(struct su_line) * (code->lines_count + count));
  if (lines == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  code->lines = (struct su_line *)lines;

  for (u2 i = 0; i < count; i++) {
    u2 start;
    struct su_line line;

    SU_TRY(status, su_stream_r2(stream, &start, 0));
    SU_TRY(status, su_stream_r2(stream, &line.line, 0));

    line.start = su_code_label(labels, start);
    if (line.start != NULL)
      code->lines[code->lines_count++] = line;
  }

  return status;
}

static enum su_error su_code_parse_locals(struct su_local **locals, u2 *locals_count, struct su_stream *stream, const struct su_code_labels *labels) {
  enum su_error status = SU_OK;

  u2 count;
  SU_TRY(status, su_stream_r2(stream, &count, 0));

  if (count > (stream->length - stream->cursor) / (sizeof(u2) * 5))
    return SU_STREAM_AT_END;

  if (count == 0)
    return status;

  void *expanded = realloc(*locals, sizeof(struct su_local) * (*locals_count + count));
  if (expanded == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  *locals = (struct su_local *)expanded;

  for (u2 i = 0; i < count; i++) {
    u2 start, length;
    struct su_local local;

    SU_TRY(status, su_stream_r2(stream, &start, 0));
    SU_TRY(status, su_stream_r2(stream, &length, 0));
    SU_TRY(status, su_stream_r2(stream, &local.name, 0));
    SU_TRY(status, su_stream_r2(stream, &local.descriptor, 0));
    SU_TRY(status, su_stream_r2(stream, &local.index, 0));

    const u4 end = (u4)start + length;

    local.start = su_code_label(labels, start);
    local.end = su_code_label(labels, end);

    if ((local.start == NULL && start != labels->length) || (local.end == NULL && end != labels->length))
      continue;

    (*locals)[(*locals_count)++] = local;
  }

  return status;
}

static enum su_error su_code_parse_items(struct su_stream *stream, const struct su_code_labels *labels, struct su_frame_item *items, const u2 count) {
  enum su_error status = SU_OK;

  for (u2 i = 0; i < count; i++) {
    struct su_frame_item *item = &items[i];

    u1 type;
    SU_TRY(status, su_stream_r1(stream, &type, 0));

    if (type > SU_ITEM_UNINITIALIZED)
      return SU_CLASS_CODE_INVALID_FRAME;

    item->type = (enum su_frame_item_type)type;

    if (type == SU_ITEM_OBJECT) {
      SU_TRY(status, su_stream_r2(stream, &item->constant, 0));
    } else if (type == SU_ITEM_UNINITIALIZED) {
      u2 pc;
      SU_TRY(status, su_stream_r2(stream, &pc, 0));

      item->insn = su_code_label(labels, pc);
      if (item->insn == NULL)
        return SU_CLASS_CODE_INVALID_FRAME;
    }
  }

  return status;
}

static enum su_error su_code_parse_frames(struct su_stream *stream, const struct su_code_labels *labels) {
  enum su_error status = SU_OK;

  u2 count;
  SU_TRY(status, su_stream_r2(stream, &count, 0));

  u4 pc = 0;
  for (u2 i = 0; i < count; i++) {
    u1 tag;
    SU_TRY(status, su_stream_r1(stream, &tag, 0));

    enum su_frame_type type = SU_FRAME_SAME;
    u2 delta = 0, locals_count = 0, stack_count = 0;
    u1 chop = 0;

    if (tag < SAME_LOCALS_1_STACK_ITEM) {
      delta = tag;
    } else if (tag < SAME_LOCALS_1_STACK_ITEM * 2) {
      type = SU_FRAME_SAME_LOCALS_1_STACK_ITEM;
      delta = tag - SAME_LOCALS_1_STACK_ITEM;
      stack_count = 1;
    } else if (tag < SAME_LOCALS_1_STACK_ITEM_EXTENDED) {
      return SU_CLASS_CODE_INVALID_FRAME;
    } else {
      SU_TRY(status, su_stream_r2(stream, &delta, 0));

      if (tag == SAME_LOCALS_1_STACK_ITEM_EXTENDED) {
        type = SU_FRAME_SAME_LOCALS_1_STACK_ITEM;
        stack_count = 1;
      } else if (tag < SAME_FRAME_EXTENDED) {
        type = SU_FRAME_CHOP;
        chop = SAME_FRAME_EXTENDED - tag;
      } else if (tag == FULL_FRAME) {
        type = SU_FRAME_FULL;
        SU_TRY(status, su_stream_r2(stream, &locals_count, 0));
      } else if (tag > SAME_FRAME_EXTENDED) {
        type = SU_FRAME_APPEND;
        locals_count = tag - SAME_FRAME_EXTENDED;
      }
    }

    pc += delta + (i > 0 ? 1 : 0);

    struct su_insn *insn = su_code_label(labels, pc);
    if (insn == NULL || insn->frame != NULL || locals_count > stream->length - stream->cursor)
      return SU_CLASS_CODE_INVALID_FRAME;

    SU_TRY(status, su_code_frame(insn, type, locals_count, stack_count));

    struct su_frame *frame = insn->frame;
    frame->chop = chop;

    SU_TRY(status, su_code_parse_items(stream, labels, frame->locals, locals_count));

    if (type == SU_FRAME_FULL) {
      SU_TRY(status, su_stream_r2(stream, &stack_count, 0));

      if (stack_count > stream->length - stream->cursor)
        return SU_CLASS_CODE_INVALID_FRAME;

      if (stack_count > 0) {
        frame->stack = calloc(stack_count, sizeof(struct su_frame_item));
        if (frame->stack == NULL)
          return SU_MEMORY_ALLOCATION_FAILURE;

        frame->stack_count = stack_count;
      }
    }

    SU_TRY(status, su_code_parse_items(stream, labels, frame->stack, stack_count));
  }

  return status;
}

static enum su_error su_code_parse_attribute(struct su_code *code, const u2 name, const struct su_stream *stream) {
  void *attributes = realloc(code->attributes, sizeof(struct su_code_attribute) * (code->attributes_count + 1));
  if (attributes == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  code->attributes = (struct su_code_attribute *)attributes;

  struct su_code_attribute *attribute = &code->attributes[code->attributes_count];
  attribute->name = name;
  attribute->length = stream->length;
  attribute->bytes = NULL;

  if (stream->length > 0) {
    attribute->bytes = malloc(stream->length);
    if (attribute->bytes == NULL)
      return SU_MEMORY_ALLOCATION_FAILURE;

    memcpy(attribute->bytes, stream->buffer, stream->length);
  }

  code->attributes_count++;
  return SU_OK;
}

static enum su_error su_code_build_exceptions(const struct su_code *code, struct su_stream *stream, const struct su_code_labels *labels) {
  enum su_error status = SU_OK;
  SU_TRY(status, su_stream_w2(stream, code->exceptions_count, 0));

  for (u2 i = 0; i < code->exceptions_count; i++) {
    const struct su_exception *exception = &code->exceptions[i];

    u2 start, end, handler;
    if (!su_code_pc(labels, exception->start, false, &start) || !su_code_pc(labels, exception->end, true, &end) || !su_code_pc(labels, exception->handler, false, &handler) || start >= end)
      return SU_CLASS_CODE_INVALID_EXCEPTION;

    SU_TRY(status, su_stream_w2(stream, start, 0));
    SU_TRY(status, su_stream_w2(stream, end, 0));
    SU_TRY(status, su_stream_w2(stream, handler, 0));
    SU_TRY(status, su_stream_w2(stream, exception->catch_type, 0));
  }

  return status;
}

static enum su_error su_code_build_lines(const struct su_code *code, struct su_stream *stream, const struct su_code_labels *labels) {
  enum su_error status = SU_OK;

  u2 attribute;
  SU_TRY(status, su_code_begin(stream, code->lines_name, &attribute));
  SU_TRY(status, su_stream_w2(stream, code->lines_count, 0));

  for (u2 i = 0; i < code->lines_count; i++) {
    u2 start;
    if (!su_code_pc(labels, code->lines[i].start, false, &start))
      return SU_CLASS_CODE_INVALID_ATTRIBUTE;

    SU_TRY(status, su_stream_w2(stream, start, 0));
    SU_TRY(status, su_stream_w2(stream, code->lines[i].line, 0));
  }

  return su_code_end(stream, attribute);
}

static enum su_error su_code_build_locals(const struct su_local *locals, const u2 locals_count, const u2 name, struct su_stream *stream, const struct su_code_labels *labels) {
  enum su_error status = SU_OK;

  u2 attribute;
  SU_TRY(status, su_code_begin(stream, name, &attribute));
  SU_TRY(status, su_stream_w2(stream, locals_count, 0));

  for (u2 i = 0; i < locals_count; i++) {
    const struct su_local *local = &locals[i];

    u2 start, end;
    if (!su_code_pc(labels, local->start, true, &start) || !su_code_pc(labels, local->end, true, &end) || start > end)
      return SU_CLASS_CODE_INVALID_ATTRIBUTE;

    SU_TRY(status, su_stream_w2(stream, start, 0));
    SU_TRY(status, su_stream_w2(stream, (u2)(end - start), 0));
    SU_TRY(status, su_stream_w2(stream, local->name, 0));
    SU_TRY(status, su_stream_w2(stream, local->descriptor, 0));
    SU_TRY(status, su_stream_w2(stream, local->index, 0));
  }

  return su_code_end(stream, attribute);
}

static enum su_error su_code_build_items(struct su_stream *stream, const struct su_code_labels *labels, const struct su_frame_item *items, const u2 count) {
  enum su_error status = SU_OK;
  if (count > 0 && items == NULL)
    return SU_CLASS_CODE_INVALID_FRAME;

  for (u2 i = 0; i < count; i++) {
    const struct su_frame_item *item = &items[i];
    if ((u4)item->type > SU_ITEM_UNINITIALIZED)
      return SU_CLASS_CODE_INVALID_FRAME;

    SU_TRY(status, su_stream_w1(stream, (u1)item->type, 0));

    if (item->type == SU_ITEM_OBJECT) {
      SU_TRY(status, su_stream_w2(stream, item->constant, 0));
    } else if (item->type == SU_ITEM_UNINITIALIZED) {
      u2 pc;
      if (!su_code_pc(labels, item->insn, false, &pc))
        return SU_CLASS_CODE_INVALID_FRAME;

      SU_TRY(status, su_stream_w2(stream, pc, 0));
    }
  }

  return status;
}

static enum su_error su_code_build_frames(const struct su_code *code, struct su_stream *stream, const struct su_code_labels *labels, const u2 count) {
  enum su_error status = SU_OK;

  u2 attribute;
  SU_TRY(status, su_code_begin(stream, code->frames_name, &attribute));
  SU_TRY(status, su_stream_w2(stream, count, 0));

  s4 previous = -1;
  for (const struct su_insn *insn = code->instructions; insn != NULL; insn = insn->next) {
    const struct su_frame *frame = insn->frame;
    if (frame == NULL)
      continue;

    const u2 delta = (u2)(insn->pc - previous - 1);
    previous = insn->pc;

    switch (frame->type) {
      case SU_FRAME_SAME:
        if (delta < SAME_LOCALS_1_STACK_ITEM) {
          SU_TRY(status, su_stream_w1(stream, (u1)delta, 0));
        } else {
          SU_TRY(status, su_stream_w1(stream, SAME_FRAME_EXTENDED, 0));
          SU_TRY(status, su_stream_w2(stream, delta, 0));
        }
        break;
      case SU_FRAME_SAME_LOCALS_1_STACK_ITEM:
        if (frame->stack_count != 1)
          return SU_CLASS_CODE_INVALID_FRAME;

        if (delta < SAME_LOCALS_1_STACK_ITEM) {
          SU_TRY(status, su_stream_w1(stream, (u1)(SAME_LOCALS_1_STACK_ITEM + delta), 0));
        } else {
          SU_TRY(status, su_stream_w1(stream, SAME_LOCALS_1_STACK_ITEM_EXTENDED, 0));
          SU_TRY(status, su_stream_w2(stream, delta, 0));
        }

        SU_TRY(status, su_code_build_items(stream, labels, frame->stack, 1));
        break;
      case SU_FRAME_CHOP:
        if (frame->chop < 1 || frame->chop > 3)
          return SU_CLASS_CODE_INVALID_FRAME;

        SU_TRY(status, su_stream_w1(stream, (u1)(SAME_FRAME_EXTENDED - frame->chop), 0));
        SU_TRY(status, su_stream_w2(stream, delta, 0));
        break;
      case SU_FRAME_APPEND:
        if (frame->locals_count < 1 || frame->locals_count > 3)
          return SU_CLASS_CODE_INVALID_FRAME;

        SU_TRY(status, su_stream_w1(stream, (u1)(SAME_FRAME_EXTENDED + frame->locals_count), 0));
        SU_TRY(status, su_stream_w2(stream, delta, 0));
        SU_TRY(status, su_code_build_items(stream, labels, frame->locals, frame->locals_count));
        break;
      case SU_FRAME_FULL:
        SU_TRY(status, su_stream_w1(stream, FULL_FRAME, 0));
        SU_TRY(status, su_stream_w2(stream, delta, 0));
        SU_TRY(status, su_stream_w2(stream, frame->locals_count, 0));
        SU_TRY(status, su_code_build_items(stream, labels, frame->locals, frame->locals_count));
        SU_TRY(status, su_stream_w2(stream, frame->stack_count, 0));
        SU_TRY(status, su_code_build_items(stream, labels, frame->stack, frame->stack_count));
        break;
      default:
        return SU_CLASS_CODE_INVALID_FRAME;
    }
  }

  return su_code_end(stream, attribute);
}

enum su_error su_code_parse(struct su_code *code, void **constant_pool, const u2 constant_pool_count, struct su_stream *stream) {
  if (code == NULL || constant_pool == NULL || stream == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  enum su_error status = SU_OK;
  struct su_code_labels labels = { 0 };

  memset(code, 0, sizeof(struct su_code));

  const u2 start = stream->cursor;

  u4 attribute_length;
  SU_TRY_CATCH(status, su_stream_r2(stream, &code->name, 0), exit);
  SU_TRY_CATCH(status, su_stream_r4(stream, &attribute_length, 0), exit);

  struct su_stream body;
  SU_TRY_CATCH(status, su_code_slice(stream, attribute_length, &body), exit);

  u4 code_length;
  SU_TRY_CATCH(status, su_stream_r2(&body, &code->max_stack, 0), exit);
  SU_TRY_CATCH(status, su_stream_r2(&body, &code->max_locals, 0), exit);
  SU_TRY_CATCH(status, su_stream_r4(&body, &code_length, 0), exit);

  SU_TRY_CATCH(status, su_disasm_parse(&code->instructions, constant_pool, constant_pool_count, &body, code_length), exit);
  SU_TRY_CATCH(status, su_code_labels_init(&labels, code->instructions, code_length), exit);
  SU_TRY_CATCH(status, su_code_parse_exceptions(code, &body, &labels), exit);

  u2 attributes_count;
  SU_TRY_CATCH(status, su_stream_r2(&body, &attributes_count, 0), exit);

  for (u2 i = 0; i < attributes_count; i++) {
    u2 name;
    SU_TRY_CATCH(status, su_stream_r2(&body, &name, 0), exit);

    u4 length;
    SU_TRY_CATCH(status, su_stream_r4(&body, &length, 0), exit);

    struct su_stream attribute;
    SU_TRY_CATCH(status, su_code_slice(&body, length, &attribute), exit);

    if (name == 0 || name >= constant_pool_count || constant_pool[name] == NULL) {
      status = SU_CLASS_CODE_INVALID_CONSTANT;
      goto exit;
    }

    const char *attribute_name = constant_pool[name];

    if (strcmp(attribute_name, "StackMapTable") == 0) {
      code->frames_name = name;
      SU_TRY_CATCH(status, su_code_parse_frames(&attribute, &labels), exit);
    } else if (strcmp(attribute_name, "LineNumberTable") == 0) {
      code->lines_name = name;
      SU_TRY_CATCH(status, su_code_parse_lines(code, &attribute, &labels), exit);
    } else if (strcmp(attribute_name, "LocalVariableTable") == 0) {
      code->locals_name = name;
      SU_TRY_CATCH(status, su_code_parse_locals(&code->locals, &code->locals_count, &attribute, &labels), exit);
    } else if (strcmp(attribute_name, "LocalVariableTypeTable") == 0) {
      code->local_types_name = name;
      SU_TRY_CATCH(status, su_code_parse_locals(&code->local_types, &code->local_types_count, &attribute, &labels), exit);
    } else if (strcmp(attribute_name, "RuntimeVisibleTypeAnnotations") == 0 || strcmp(attribute_name, "RuntimeInvisibleTypeAnnotations") == 0) {
      // these are full of offsets into the code and the jvm does not read
      // them, so they are dropped instead of being kept with stale offsets
      continue;
    } else {
      SU_TRY_CATCH(status, su_code_parse_attribute(code, name, &attribute), exit);
    }
  }

exit:
  SU_FREE(labels.table);

  if (status != SU_OK) {
    su_code_dispose(code);
    stream->cursor = start;
  }

  return status;
}

enum su_error su_code_build(const struct su_code *code, struct su_stream *stream) {
  if (code == NULL || stream == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  enum su_error status = SU_OK;
  struct su_code_labels labels = { 0 };

  const u2 start = stream->cursor;

  u4 max_locals = code->max_locals;
  u4 frames_count = 0;

  for (const struct su_insn *insn = code->instructions; insn != NULL; insn = insn->next) {
    u4 required = 0;
    if (insn->type == SU_INSN_VAR) {
      const bool wide = insn->opcode == LLOAD || insn->opcode == DLOAD || insn->opcode == LSTORE || insn->opcode == DSTORE;
      required = insn->var + (wide ? 2 : 1);
    } else if (insn->type == SU_INSN_IINC) {
      required = insn->iinc.var + 1;
    }

    if (required > max_locals)
      max_locals = required;

    if (insn->frame != NULL)
      frames_count++;
  }

  if (max_locals > UINT16_MAX)
    return SU_CLASS_CODE_INVALID_OPERAND;

  const bool lines = code->lines_count > 0 || code->lines_name != 0;
  const bool locals = code->locals_count > 0 || code->locals_name != 0;
  const bool local_types = code->local_types_count > 0 || code->local_types_name != 0;
  const bool frames = frames_count > 0 || code->frames_name != 0;

  const u2 attributes_count = code->attributes_count + lines + locals + local_types + frames;

  u2 attribute;
  SU_TRY_CATCH(status, su_code_begin(stream, code->name, &attribute), exit);
  SU_TRY_CATCH(status, su_stream_w2(stream, code->max_stack, 0), exit);
  SU_TRY_CATCH(status, su_stream_w2(stream, (u2)max_locals, 0), exit);

  const u2 code_length_position = stream->cursor;
  SU_TRY_CATCH(status, su_stream_w4(stream, 0, 0), exit);

  u4 code_length;
  SU_TRY_CATCH(status, su_asm_build(code->instructions, stream, &code_length), exit);
  SU_TRY_CATCH(status, su_code_patch(stream, code_length_position, code_length), exit);

  SU_TRY_CATCH(status, su_code_labels_init(&labels, code->instructions, code_length), exit);
  SU_TRY_CATCH(status, su_code_build_exceptions(code, stream, &labels), exit);
  SU_TRY_CATCH(status, su_stream_w2(stream, attributes_count, 0), exit);

  if (lines) {
    SU_TRY_CATCH(status, su_code_build_lines(code, stream, &labels), exit);
  }

  if (locals) {
    SU_TRY_CATCH(status, su_code_build_locals(code->locals, code->locals_count, code->locals_name, stream, &labels), exit);
  }

  if (local_types) {
    SU_TRY_CATCH(status, su_code_build_locals(code->local_types, code->local_types_count, code->local_types_name, stream, &labels), exit);
  }

  if (frames) {
    SU_TRY_CATCH(status, su_code_build_frames(code, stream, &labels, (u2)frames_count), exit);
  }

  for (u2 i = 0; i < code->attributes_count; i++) {
    const struct su_code_attribute *raw = &code->attributes[i];

    SU_TRY_CATCH(status, su_stream_w2(stream, raw->name, 0), exit);
    SU_TRY_CATCH(status, su_stream_w4(stream, raw->length, 0), exit);

    if (raw->length > 0) {
      SU_TRY_CATCH(status, su_stream_wn(stream, raw->bytes, raw->length, 0), exit);
    }
  }

  SU_TRY_CATCH(status, su_code_end(stream, attribute), exit);

exit:
  SU_FREE(labels.table);

  if (status != SU_OK)
    stream->cursor = start;

  return status;
}

enum su_error su_code_frame(struct su_insn *insn, const enum su_frame_type type, const u2 locals_count, const u2 stack_count) {
  if (insn == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  struct su_frame *frame = calloc(1, sizeof(struct su_frame));
  if (frame == NULL)
    return SU_MEMORY_ALLOCATION_FAILURE;

  frame->type = type;

  if (locals_count > 0)
    frame->locals = calloc(locals_count, sizeof(struct su_frame_item));

  if (stack_count > 0)
    frame->stack = calloc(stack_count, sizeof(struct su_frame_item));

  if ((locals_count > 0 && frame->locals == NULL) || (stack_count > 0 && frame->stack == NULL)) {
    SU_FREE(frame->locals);
    SU_FREE(frame->stack);
    SU_FREE(frame);
    return SU_MEMORY_ALLOCATION_FAILURE;
  }

  frame->locals_count = locals_count;
  frame->stack_count = stack_count;

  su_code_frame_dispose(insn);
  insn->frame = frame;
  return SU_OK;
}

void su_code_frame_dispose(struct su_insn *insn) {
  if (insn == NULL || insn->frame == NULL)
    return;

  SU_FREE(insn->frame->locals);
  SU_FREE(insn->frame->stack);
  SU_FREE(insn->frame);
}

void su_code_dispose(struct su_code *code) {
  if (code == NULL)
    return;

  su_disasm_dispose(code->instructions);
  code->instructions = NULL;

  for (u2 i = 0; i < code->attributes_count; i++)
    SU_FREE(code->attributes[i].bytes);

  SU_FREE(code->attributes);
  SU_FREE(code->exceptions);
  SU_FREE(code->lines);
  SU_FREE(code->locals);
  SU_FREE(code->local_types);

  code->attributes_count = 0;
  code->exceptions_count = 0;
  code->lines_count = 0;
  code->locals_count = 0;
  code->local_types_count = 0;
}
