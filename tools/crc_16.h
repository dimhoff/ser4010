/**
 * crc_16.h - Simple/Naive CRC-16 implementation
 *
 * Written 2014, David Imhoff <dimhoff.devel@gmail.com>
 *
 * This is free and unencumbered software released into the public domain.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __CRC_16_H__
#define __CRC_16_H__

#include <stdint.h>

/**
 * Calculate next CRC-16 state
 *
 * Calculate the next CRC-16 value given input byte 'b'. To calculate a CRC over
 * a buffer, first set the crc to its initial value. Then loop through the
 * buffer calling crc_16 on every byte, passing the output of the previous call
 * as 'crc' input. The output of the last call is the actual CRC.
 *
 * @param crc		Current CRC state
 * @param b		Byte to calculate CRC on
 * @param polynomial	The polynomial to use
 *
 * @returns	New CRC state/Final CRC value
 */
uint16_t crc_16(uint16_t crc, uint8_t b, uint16_t polynomial);

#endif // __CRC_16_H__
