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

struct su_chunk {
  struct su_stream stream;

  struct su_chunk *next;
  struct su_chunk *prev;
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

enum su_error su_stream_chunk(struct su_stream *stream, struct su_chunk **chunks, struct su_chunk** chunk_out);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_STREAM_H
