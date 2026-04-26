#include <suture/stream.h>

#include "internal.h"

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#  define bswap_16(x) (x)
#  define bswap_32(x) (x)
#  define bswap_64(x) (x)
#elif defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
#  define bswap_16(x) __builtin_bswap16(x)
#  define bswap_32(x) __builtin_bswap32(x)
#  define bswap_64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
#  include <stdlib.h>
#  define bswap_16(x) _byteswap_ushort(x)
#  define bswap_32(x) _byteswap_ulong(x)
#  define bswap_64(x) _byteswap_uint64(x)
#elif defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define bswap_16(x) OSSwapInt16(x)
#  define bswap_32(x) OSSwapInt32(x)
#  define bswap_64(x) OSSwapInt64(x)
#else
#  include <byteswap.h>
#endif

enum su_error su_stream_rn(struct su_stream *stream, u1 *buffer, const u2 size, const u2 offset) {
  if (stream == NULL || buffer == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (offset > stream->length - stream->cursor ||
      size > stream->length - stream->cursor - offset)
    return SU_STREAM_AT_END;

  stream->cursor += offset;
  memcpy(buffer, stream->buffer + stream->cursor, size);
  stream->cursor += size;

  return SU_OK;
}

enum su_error su_stream_r1(struct su_stream *stream, u1 *value, const u2 offset) {
  if (stream == NULL || value == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  const u2 type_size = sizeof(u1);
  if (type_size > stream->length - stream->cursor ||
      offset > stream->length - stream->cursor - type_size)
    return SU_STREAM_AT_END;

  stream->cursor += offset;
  (*value) = (u1)stream->buffer[stream->cursor];

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r2(struct su_stream *stream, u2 *value, const u2 offset) {
  if (stream == NULL || value == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  const u2 type_size = sizeof(u2);
  if (type_size > stream->length - stream->cursor ||
      offset > stream->length - stream->cursor - type_size)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u2 intern = *(u2 *)(stream->buffer + stream->cursor);
  (*value) = bswap_16(intern);

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r4(struct su_stream *stream, u4 *value, const u2 offset) {
  if (stream == NULL || value == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  const u2 type_size = sizeof(u4);
  if (type_size > stream->length - stream->cursor ||
      offset > stream->length - stream->cursor - type_size)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u4 intern = *(u4 *)(stream->buffer + stream->cursor);
  (*value) = bswap_32(intern);

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r8(struct su_stream *stream, u8 *value, const u2 offset) {
  if (stream == NULL || value == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  const u2 type_size = sizeof(u8);
  if (type_size > stream->length - stream->cursor ||
      offset > stream->length - stream->cursor - type_size)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u8 intern = *(u8 *)(stream->buffer + stream->cursor);
  (*value) = bswap_64(intern);

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_wn(struct su_stream *stream, const u1 *buffer, const u2 size, const u2 offset) {
  if (stream == NULL || buffer == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (size > stream->length - stream->cursor || offset > stream->length - stream->cursor - size) {
    const size_t length = stream->cursor + size + offset;
    void *bigger_buffer = realloc(stream->buffer, length);

    if (!bigger_buffer)
      return SU_MEMORY_ALLOCATION_FAILURE;

    stream->buffer = (u1 *)bigger_buffer;
    stream->length = length;
  }

  stream->cursor += offset;
  memcpy(stream->buffer + stream->cursor, buffer, size);
  stream->cursor += size;

  return SU_OK;
}

enum su_error su_stream_w1(struct su_stream *stream, const u1 value, const u2 offset) {
  if (stream == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  const u2 type_size = sizeof(u1);
  if (type_size > stream->length - stream->cursor || offset > stream->length - stream->cursor - type_size) {
    const size_t length = stream->cursor + type_size + offset;
    void *bigger_buffer = realloc(stream->buffer, length);

    if (!bigger_buffer)
      return SU_MEMORY_ALLOCATION_FAILURE;

    stream->buffer = bigger_buffer;
    stream->length = length;
  }

  stream->cursor += offset;
  *(u1 *)(stream->buffer + stream->cursor) = value;
  stream->cursor += type_size;

  return SU_OK;
}

enum su_error su_stream_w2(struct su_stream *stream, const u2 value, const u2 offset) {
  if (stream == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  const u2 type_size = sizeof(u2);
  if (type_size > stream->length - stream->cursor || offset > stream->length - stream->cursor - type_size) {
    const size_t length = stream->cursor + type_size + offset;
    void *bigger_buffer = realloc(stream->buffer, length);

    if (!bigger_buffer)
      return SU_MEMORY_ALLOCATION_FAILURE;

    stream->buffer = bigger_buffer;
    stream->length = length;
  }

  stream->cursor += offset;
  *(u2 *)(stream->buffer + stream->cursor) = bswap_16(value);
  stream->cursor += type_size;

  return SU_OK;
}

enum su_error su_stream_w4(struct su_stream *stream, const u4 value, const u2 offset) {
  if (stream == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  const u2 type_size = sizeof(u4);
  if (type_size > stream->length - stream->cursor || offset > stream->length - stream->cursor - type_size) {
    const size_t length = stream->cursor + type_size + offset;
    void *bigger_buffer = realloc(stream->buffer, length);

    if (!bigger_buffer)
      return SU_MEMORY_ALLOCATION_FAILURE;

    stream->buffer = bigger_buffer;
    stream->length = length;
  }

  stream->cursor += offset;
  *(u4 *)(stream->buffer + stream->cursor) = bswap_32(value);
  stream->cursor += type_size;

  return SU_OK;
}

enum su_error su_stream_w8(struct su_stream *stream, const u8 value, const u2 offset) {
  if (stream == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  const u2 type_size = sizeof(u8);
  if (type_size > stream->length - stream->cursor || offset > stream->length - stream->cursor - type_size) {
    const size_t length = stream->cursor + type_size + offset;
    void *bigger_buffer = realloc(stream->buffer, length);

    if (!bigger_buffer)
      return SU_MEMORY_ALLOCATION_FAILURE;

    stream->buffer = bigger_buffer;
    stream->length = length;
  }

  stream->cursor += offset;
  *(u8 *)(stream->buffer + stream->cursor) = bswap_64(value);
  stream->cursor += type_size;

  return SU_OK;
}

enum su_error su_stream_chunk(struct su_stream *stream, struct su_chunk **chunks, struct su_chunk **chunk_out) {
  if (stream == NULL || chunks == NULL)
    return SU_MISSING_REQUIRED_PARAMETERS;

  if (stream->cursor < stream->chunk)
    return SU_STREAM_INVALID_CHUNK;

  const size_t chunk_size = stream->cursor - stream->chunk;

  u1 *buffer = NULL;
  if (chunk_size > 0) {
    buffer = (u1 *)malloc(sizeof(u1) * chunk_size);
    if (buffer == NULL)
      return SU_MEMORY_ALLOCATION_FAILURE;
    memcpy(buffer, stream->buffer + stream->chunk, chunk_size);
  }

  struct su_chunk *chunk = (struct su_chunk *)malloc(sizeof(struct su_chunk));
  if (chunk == NULL) {
    free(buffer);
    return SU_MEMORY_ALLOCATION_FAILURE;
  }

  chunk->next = NULL;
  chunk->prev = NULL;

  chunk->stream = (struct su_stream){
    .buffer = buffer,
    .length = chunk_size,
    .cursor = chunk_size,
    .chunk = 0
  };

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