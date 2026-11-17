#ifndef SUTURE_ASM_H
#define SUTURE_ASM_H

#include "disasm.h"
#include "error.h"
#include "stream.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_asm_insn(struct su_insn **insn, u1 opcode);

enum su_error su_asm_insert_before(struct su_insn **instructions, struct su_insn *position, struct su_insn *insn);

enum su_error su_asm_insert_after(struct su_insn **instructions, struct su_insn *position, struct su_insn *insn);

enum su_error su_asm_build(struct su_insn *instructions, struct su_stream *stream, u4 *code_length);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_ASM_H
