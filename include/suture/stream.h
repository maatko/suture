#ifndef SUTURE_STREAM_H
#define SUTURE_STREAM_H

#include "error.h"
#include "types.h"

struct su_stream {
  u1 *buffer;
  u2 cursor;
  u2 length;
  u2 chunk;
};

#ifdef __cplusplus
extern "C" {
#endif

enum su_error su_stream_rn(struct su_stream *stream, u1 *buffer, u2 size, u2 offset);

enum su_error su_stream_r1(struct su_stream *stream, u1 *value, u2 offset);

enum su_error su_stream_r2(struct su_stream *stream, u2 *value, u2 offset);

enum su_error su_stream_r4(struct su_stream *stream, u4 *value, u2 offset);

enum su_error su_stream_r8(struct su_stream *stream, u8 *value, u2 offset);

void su_stream_wn(struct su_stream *stream, const u1 *buffer, u2 size, u2 offset);

void su_stream_w1(struct su_stream *stream, u1 value, u2 offset);

void su_stream_w2(struct su_stream *stream, u2 value, u2 offset);

void su_stream_w4(struct su_stream *stream, u4 value, u2 offset);

void su_stream_w8(struct su_stream *stream, u8 value, u2 offset);

enum su_error su_stream_chunk(struct su_stream *stream, struct su_stream **chunks, u2 *chunks_count);

#ifdef __cplusplus
}
#endif

#define su_stream_read(stream, T, value, offset) _Generic((T){ 0 }, \
    u1: su_stream_r1,                                               \
    u2: su_stream_r2,                                               \
    u4: su_stream_r4,                                               \
    u8: su_stream_r8)(stream, value, offset)

#define su_stream_write(stream, T, value, offset) _Generic((T){ 0 }, \
    u1: su_stream_w1,                                                \
    u2: su_stream_w2,                                                \
    u4: su_stream_w4,                                                \
    u8: su_stream_w8)(stream, value, offset)

#endif // SUTURE_STREAM_H
