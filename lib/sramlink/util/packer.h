/**
 * @file packer.h
 * @brief Binary packing/unpacking utilities for SRAMLink messages
 *
 * Ported from bambam for nRF Connect SDK.
 */
#pragma once

#include <stdint.h>
#include <string.h>

/**
 * @brief Pack a uint8_t into buffer
 */
static inline void pack_uint8(uint8_t *buffer, uint8_t *offset, uint8_t value)
{
    buffer[(*offset)++] = value;
}

/**
 * @brief Unpack a uint8_t from buffer
 */
static inline uint8_t unpack_uint8(const uint8_t *buffer, uint8_t *offset)
{
    return buffer[(*offset)++];
}

/**
 * @brief Pack a uint16_t (big-endian) into buffer
 */
static inline void pack_be_uint16(uint8_t *buffer, uint8_t *offset, uint16_t value)
{
    buffer[(*offset)++] = (uint8_t)(value >> 8);
    buffer[(*offset)++] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Unpack a uint16_t (big-endian) from buffer
 */
static inline uint16_t unpack_be_uint16(const uint8_t *buffer, uint8_t *offset)
{
    uint16_t value;
    value = (uint16_t)buffer[(*offset)++] << 8;
    value |= buffer[(*offset)++];
    return value;
}

/**
 * @brief Pack a uint32_t (big-endian) into buffer
 */
static inline void pack_be_uint32(uint8_t *buffer, uint8_t *offset, uint32_t value)
{
    buffer[(*offset)++] = (uint8_t)(value >> 24);
    buffer[(*offset)++] = (uint8_t)(value >> 16);
    buffer[(*offset)++] = (uint8_t)(value >> 8);
    buffer[(*offset)++] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Unpack a uint32_t (big-endian) from buffer
 */
static inline uint32_t unpack_be_uint32(const uint8_t *buffer, uint8_t *offset)
{
    uint32_t value;
    value = (uint32_t)buffer[(*offset)++] << 24;
    value |= (uint32_t)buffer[(*offset)++] << 16;
    value |= (uint32_t)buffer[(*offset)++] << 8;
    value |= buffer[(*offset)++];
    return value;
}

/**
 * @brief Pack a uint8_t array into buffer
 */
static inline void pack_uint8_array(uint8_t *buffer, uint8_t *offset,
                                    const uint8_t *source, uint8_t length)
{
    memcpy(&buffer[*offset], source, length);
    *offset += length;
}

/**
 * @brief Unpack a uint8_t array from buffer
 */
static inline void unpack_uint8_array(const uint8_t *buffer, uint8_t *offset,
                                      uint8_t *dest, uint8_t length)
{
    memcpy(dest, &buffer[*offset], length);
    *offset += length;
}

/**
 * @brief Pack a uint16_t (little-endian) into buffer
 */
static inline void pack_le_uint16(uint8_t *buffer, uint8_t *offset, uint16_t value)
{
    buffer[(*offset)++] = (uint8_t)(value & 0xFF);
    buffer[(*offset)++] = (uint8_t)(value >> 8);
}

/**
 * @brief Unpack a uint16_t (little-endian) from buffer
 */
static inline uint16_t unpack_le_uint16(const uint8_t *buffer, uint8_t *offset)
{
    uint16_t value;
    value = buffer[(*offset)++];
    value |= (uint16_t)buffer[(*offset)++] << 8;
    return value;
}

/**
 * @brief Pack a uint32_t (little-endian) into buffer
 */
static inline void pack_le_uint32(uint8_t *buffer, uint8_t *offset, uint32_t value)
{
    buffer[(*offset)++] = (uint8_t)(value & 0xFF);
    buffer[(*offset)++] = (uint8_t)(value >> 8);
    buffer[(*offset)++] = (uint8_t)(value >> 16);
    buffer[(*offset)++] = (uint8_t)(value >> 24);
}

/**
 * @brief Unpack a uint32_t (little-endian) from buffer
 */
static inline uint32_t unpack_le_uint32(const uint8_t *buffer, uint8_t *offset)
{
    uint32_t value;
    value = buffer[(*offset)++];
    value |= (uint32_t)buffer[(*offset)++] << 8;
    value |= (uint32_t)buffer[(*offset)++] << 16;
    value |= (uint32_t)buffer[(*offset)++] << 24;
    return value;
}

/* Signed type convenience macros */
#define pack_int8(b, o, v)         pack_uint8(b, o, (uint8_t)(v))
#define unpack_int8(b, o)          ((int8_t)unpack_uint8(b, o))

#define pack_be_int16(b, o, v)     pack_be_uint16(b, o, (uint16_t)(v))
#define unpack_be_int16(b, o)      ((int16_t)unpack_be_uint16(b, o))

#define pack_be_int32(b, o, v)     pack_be_uint32(b, o, (uint32_t)(v))
#define unpack_be_int32(b, o)      ((int32_t)unpack_be_uint32(b, o))

#define pack_le_int16(b, o, v)     pack_le_uint16(b, o, (uint16_t)(v))
#define unpack_le_int16(b, o)      ((int16_t)unpack_le_uint16(b, o))

#define pack_le_int32(b, o, v)     pack_le_uint32(b, o, (uint32_t)(v))
#define unpack_le_int32(b, o)      ((int32_t)unpack_le_uint32(b, o))
