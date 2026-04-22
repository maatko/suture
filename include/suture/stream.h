/**
 * @file stream.h
 * @brief Byte-level stream I/O and chunked buffer interface.
 */

#ifndef SUTURE_STREAM_H
#define SUTURE_STREAM_H

#include "error.h"
#include "types.h"

/**
 * @brief Byte buffer with cursor-based access.
 */
struct su_stream {
  u1 *buffer; /**< Underlying byte buffer. */
  u2 cursor;  /**< Current read/write position. */
  u2 length;  /**< Total capacity of @c buffer in bytes. */
  u2 chunk;   /**< Offset to the previous chunk */
};

/**
 * @brief A doubly-linked list node wrapping an @ref su_stream for chunked processing.
 */
struct su_chunk {
  struct su_stream stream; /**< Embedded stream for this chunk's data. */

  struct su_chunk *next; /**< Next chunk in the list, or NULL if last. */
  struct su_chunk *prev; /**< Previous chunk in the list, or NULL if first. */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads @p size bytes from @p stream at @p offset into @p buffer.
 *
 * @param stream  Source stream. Must not be NULL.
 * @param buffer  Destination buffer. Must not be NULL.
 * @param size    Number of bytes to read.
 * @param offset  Byte offset within the stream to read from.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the read exceeds stream bounds.
 */
enum su_error su_stream_rn(struct su_stream *stream, u1 *buffer, u2 size, u2 offset);

/**
 * @brief Reads a @c u1 (1 byte) from @p stream at @p offset.
 *
 * @param stream Source stream. Must not be NULL.
 * @param value  Out parameter for the read value. Must not be NULL.
 * @param offset Byte offset within the stream.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the read exceeds stream bounds.
 */
enum su_error su_stream_r1(struct su_stream *stream, u1 *value, u2 offset);

/**
 * @brief Reads a @c u2 (2 bytes) from @p stream at @p offset.
 *
 * @param stream Source stream. Must not be NULL.
 * @param value  Out parameter for the read value. Must not be NULL.
 * @param offset Byte offset within the stream.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the read exceeds stream bounds.
 */
enum su_error su_stream_r2(struct su_stream *stream, u2 *value, u2 offset);

/**
 * @brief Reads a @c u4 (4 bytes) from @p stream at @p offset.
 *
 * @param stream Source stream. Must not be NULL.
 * @param value  Out parameter for the read value. Must not be NULL.
 * @param offset Byte offset within the stream.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the read exceeds stream bounds.
 */
enum su_error su_stream_r4(struct su_stream *stream, u4 *value, u2 offset);

/**
 * @brief Reads a @c u8 (8 bytes) from @p stream at @p offset.
 *
 * @param stream Source stream. Must not be NULL.
 * @param value  Out parameter for the read value. Must not be NULL.
 * @param offset Byte offset within the stream.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the read exceeds stream bounds.
 */
enum su_error su_stream_r8(struct su_stream *stream, u8 *value, u2 offset);

/**
 * @brief Writes @p size bytes from @p buffer into @p stream at @p offset.
 *
 * @param stream Source stream. Must not be NULL.
 * @param buffer Source buffer to write from. Must not be NULL.
 * @param size   Number of bytes to write.
 * @param offset Byte offset within the stream to write to.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the write exceeds stream bounds.
 */
enum su_error su_stream_wn(struct su_stream *stream, const u1 *buffer, u2 size, u2 offset);

/**
 * @brief Writes a @c u1 (1 byte) into @p stream at @p offset.
 *
 * @param stream Target stream. Must not be NULL.
 * @param value  Value to write.
 * @param offset Byte offset within the stream.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the write exceeds stream bounds.
 */
enum su_error su_stream_w1(struct su_stream *stream, u1 value, u2 offset);

/**
 * @brief Writes a @c u2 (2 bytes) into @p stream at @p offset.
 *
 * @param stream Target stream. Must not be NULL.
 * @param value  Value to write.
 * @param offset Byte offset within the stream.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the write exceeds stream bounds.
 */
enum su_error su_stream_w2(struct su_stream *stream, u2 value, u2 offset);

/**
 * @brief Writes a @c u4 (4 bytes) into @p stream at @p offset.
 *
 * @param stream Target stream. Must not be NULL.
 * @param value  Value to write.
 * @param offset Byte offset within the stream.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the write exceeds stream bounds.
 */
enum su_error su_stream_w4(struct su_stream *stream, u4 value, u2 offset);

/**
 * @brief Writes a @c u8 (8 bytes) into @p stream at @p offset.
 *
 * @param stream Target stream. Must not be NULL.
 * @param value  Value to write.
 * @param offset Byte offset within the stream.
 *
 * @return @ref SU_OK                   on success.
 * @return @ref SU_STREAM_INVALID_CHUNK if the write exceeds stream bounds.
 */
enum su_error su_stream_w8(struct su_stream *stream, u8 value, u2 offset);

/**
 * @brief Advances the stream by one chunk, allocating and appending a new @ref su_chunk.
 *
 * Allocates a new chunk, links it into the @p chunks list, and updates @p stream
 * to point to the new chunk's embedded stream for subsequent operations.
 *
 * @param stream    Current stream to advance from. Must not be NULL.
 * @param chunks    Head of the chunk linked list. Must not be NULL.
 * @param chunk_out Out parameter receiving the newly allocated chunk. Must not be NULL.
 *
 * @return @ref SU_OK                        on success.
 * @return @ref SU_MEMORY_ALLOCATION_FAILURE if the new chunk could not be allocated.
 */
enum su_error su_stream_chunk(struct su_stream *stream, struct su_chunk **chunks, struct su_chunk **chunk_out);

#ifdef __cplusplus
}
#endif

#endif // SUTURE_STREAM_H
