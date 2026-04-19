#include <suture/stream.h>

#include <assert.h>
#include <string.h>

enum su_error su_stream_rn(struct su_stream *stream, u1 *buffer, const u2 size, const u2 offset) {
  assert(stream != NULL && "su_stream_r1: `stream` must be a valid pointer");
  assert(stream != NULL && "su_stream_r1: `value` must be a valid pointer");

  if (offset + size > stream->length - stream->cursor)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  memcpy(buffer, stream->buffer + stream->cursor, size);

  stream->cursor += size;

  return SU_OK;
}

enum su_error su_stream_r1(struct su_stream *stream, u1 *value, const u2 offset) {
  assert(stream != NULL && "su_stream_r1: `stream` must be a valid pointer");
  assert(value != NULL && "su_stream_r1: `value` must be a valid pointer");

  const u2 type_size = sizeof(u1);
  if (stream->cursor + type_size + offset >= stream->length)
    return SU_STREAM_AT_END;

  stream->cursor += offset;
  (*value) = (u1)stream->buffer[stream->cursor];

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r2(struct su_stream *stream, u2 *value, const u2 offset) {
  assert(stream != NULL && "su_stream_r2: `stream` must be a valid pointer");
  assert(value != NULL && "su_stream_r2: `value` must be a valid pointer");

  const u2 type_size = sizeof(u2);
  if (stream->cursor + type_size + offset >= stream->length)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u2 intern = *(u2 *)(stream->buffer + stream->cursor);
  (*value) = bswap_16(intern);

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r4(struct su_stream *stream, u4 *value, const u2 offset) {
  assert(stream != NULL && "su_stream_r4: `stream` must be a valid pointer");
  assert(value != NULL && "su_stream_r4: `value` must be a valid pointer");

  const u2 type_size = sizeof(u4);
  if (stream->cursor + type_size + offset >= stream->length)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u4 intern = *(u4 *)(stream->buffer + stream->cursor);
  (*value) = bswap_32(intern);

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r8(struct su_stream *stream, u8 *value, const u2 offset) {
  assert(stream != NULL && "su_stream_r8: `stream` must be a valid pointer");
  assert(value != NULL && "su_stream_r8: `value` must be a valid pointer");

  const u2 type_size = sizeof(u8);
  if (stream->cursor + type_size + offset >= stream->length)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u8 intern = *(u8 *)(stream->buffer + stream->cursor);
  (*value) = bswap_64(intern);

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_chunk(struct su_stream *stream, struct su_stream **chunks, u2 *chunks_count) {
  assert(stream != NULL && "su_stream_chunk: `stream` must be a valid pointer");
  assert(chunks != NULL && "su_stream_chunk: `chunks` must be a valid pointer");

  const u2 chunk_size = stream->cursor - stream->chunk;
  if (chunk_size <= 0 || chunk_size >= stream->length)
    return SU_STREAM_INVALID_CHUNK;

  const int new_count = (*chunks_count) + 1;
  (*chunks) = realloc((void *)*chunks, (new_count * sizeof(struct su_stream)));
  assert((*chunks) != NULL && "su_stream_chunk: failed to allocate memory for chunks");

  u1 *buffer = (u1 *)malloc(sizeof(u1) * chunk_size);
  assert(buffer != NULL && "su_stream_chunk: failed to allocate memory for a stream chunk");

  memcpy(buffer, stream->buffer + stream->chunk, chunk_size);

  (*chunks)[new_count - 1] = (struct su_stream){
    .buffer = buffer,
    .length = chunk_size,
    .cursor = 0,
    .chunk = 0
  };

  stream->chunk = stream->cursor;

  (*chunks_count) = new_count;
  return SU_OK;
}