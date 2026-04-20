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

void su_stream_wn(struct su_stream *stream, const u1 *buffer, const u2 size, const u2 offset) {
  if (stream->cursor + size + offset >= stream->length) {
    stream->length = stream->cursor + size + offset;
    stream->buffer = realloc(stream->buffer, stream->length);
  }

  stream->cursor += offset;

  memcpy(stream->buffer + stream->cursor, buffer, size);

  stream->cursor += size;
}

void su_stream_w1(struct su_stream *stream, const u1 value, const u2 offset) {
  const u2 type_size = sizeof(u1);
  if (stream->cursor + type_size + offset >= stream->length) {
    stream->length = stream->cursor + type_size + offset;
    stream->buffer = realloc(stream->buffer, stream->length);
  }

  stream->cursor += offset;

  *(u1 *)(stream->buffer + stream->cursor) = value;

  stream->cursor += type_size;
}

void su_stream_w2(struct su_stream *stream, const u2 value, const u2 offset) {
  const u2 type_size = sizeof(u2);
  if (stream->cursor + type_size + offset >= stream->length) {
    stream->length = stream->cursor + type_size + offset;
    stream->buffer = realloc(stream->buffer, stream->length);
  }

  stream->cursor += offset;

  *(u2 *)(stream->buffer + stream->cursor) = bswap_16(value);

  stream->cursor += type_size;
}

void su_stream_w4(struct su_stream *stream, const u4 value, const u2 offset) {
  const u2 type_size = sizeof(u4);
  if (stream->cursor + type_size + offset >= stream->length) {
    stream->length = stream->cursor + type_size + offset;
    stream->buffer = realloc(stream->buffer, stream->length);
  }

  stream->cursor += offset;

  *(u4 *)(stream->buffer + stream->cursor) = bswap_32(value);

  stream->cursor += type_size;
}

void su_stream_w8(struct su_stream *stream, const u8 value, const u2 offset) {
  const u2 type_size = sizeof(u8);
  if (stream->cursor + type_size + offset >= stream->length) {
    stream->length = stream->cursor + type_size + offset;
    stream->buffer = realloc(stream->buffer, stream->length);
  }

  stream->cursor += offset;

  *(u8 *)(stream->buffer + stream->cursor) = bswap_64(value);

  stream->cursor += type_size;
}

enum su_error su_stream_chunk(struct su_stream *stream, struct su_chunk **chunks, struct su_chunk **chunk_out) {
  assert(stream != NULL && "su_stream_chunk: `stream` must be a valid pointer");
  assert(chunks != NULL && "su_stream_chunk: `chunks` must be a valid pointer");

  const u2 chunk_size = stream->cursor - stream->chunk;
  if (chunk_size <= 0 || chunk_size >= stream->length)
    return SU_STREAM_INVALID_CHUNK;

  u1 *buffer = (u1 *)malloc(sizeof(u1) * chunk_size);
  assert(buffer != NULL && "su_stream_chunk: failed to allocate memory for a stream chunk");

  memcpy(buffer, stream->buffer + stream->chunk, chunk_size);

  struct su_chunk *chunk = (struct su_chunk *)malloc(sizeof(struct su_chunk));
  assert(chunk != NULL && "su_stream_chunk: failed to allocate memory for a chunk");

  {
    chunk->next = NULL;
    chunk->prev = NULL;

    chunk->stream = (struct su_stream){
      .buffer = buffer,
      .length = chunk_size,
      .cursor = chunk_size,
      .chunk = 0
    };
  }

  stream->chunk = stream->cursor;

  if ((*chunks) == NULL) {
    (*chunks) = chunk;
    if (chunk_out != NULL)
      (*chunk_out) = chunk;
    return SU_OK;
  }

  struct su_chunk *last_chunk = (*chunks);
  while (last_chunk->next != NULL)
    last_chunk = last_chunk->next;

  chunk->prev = last_chunk;
  last_chunk->next = chunk;

  if (chunk_out != NULL)
    (*chunk_out) = chunk;
  return SU_OK;
}