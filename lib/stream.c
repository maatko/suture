#include <suture/stream.h>

enum su_error su_stream_r1(struct su_stream *stream, u1 *value, const u2 offset) {
  const u2 type_size = sizeof(u1);
  if (type_size + offset >= stream->length)
    return SU_STREAM_AT_END;

  stream->cursor += offset;
  (*value) = (u1)stream->buffer[stream->cursor];

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r2(struct su_stream *stream, u2 *value, const u2 offset) {
  const u2 type_size = sizeof(u2);
  if (type_size + offset >= stream->length)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u2 intern = *(u2 *)(stream->buffer + stream->cursor);
  (*value) = bswap_16(intern);

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r4(struct su_stream *stream, u4 *value, const u2 offset) {
  const u2 type_size = sizeof(u4);
  if (type_size + offset >= stream->length)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u4 intern = *(u4 *)(stream->buffer + stream->cursor);
  (*value) = bswap_32(intern);

  stream->cursor += type_size;
  return SU_OK;
}

enum su_error su_stream_r8(struct su_stream *stream, u8 *value, const u2 offset) {
  const u2 type_size = sizeof(u8);
  if (type_size + offset >= stream->length)
    return SU_STREAM_AT_END;

  stream->cursor += offset;

  const u8 intern = *(u8 *)(stream->buffer + stream->cursor);
  (*value) = bswap_64(intern);

  stream->cursor += type_size;
  return SU_OK;
}