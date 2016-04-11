/**
 * serco_defines.h - Defines for the Serial frame based protocol
 *
 * Copyright (c) 2014, David Imhoff <dimhoff.devel@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the names of its contributors may
 *       be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __SERCO_DEFINES_H__
#define __SERCO_DEFINES_H__

// SER4010 firmware version info
#define SER4010_DEV_TYPE 0x0100
#define SER4010_DEV_REV  0x0003

// Byte stuffing bytes
#define STUFF_BYTE1 0xFF
#define STUFF_BYTE2 0xAF

// Magic send cookie; used to prevent accidental sends
#define SEND_COOKIE_0 0xca
#define SEND_COOKIE_1 0x4f
#define SEND_COOKIE_2 0x09
#define SEND_COOKIE_3 0x96

// Command frame bytes
#define CMD_ID      0
#define CMD_OPCODE  1
#define CMD_PAYLOAD 2

// Command opcodes
#define CMD_RESERVED     STUFF_BYTE1
#define CMD_NOP          0
#define CMD_DEV_TYPE     1
#define CMD_DEV_REV      2

#define CMD_GET_ODS      10
#define CMD_SET_ODS      11
#define CMD_GET_PA       12
#define CMD_SET_PA       13
#define CMD_GET_FREQ     14
#define CMD_SET_FREQ     15
#define CMD_GET_FDEV     16
#define CMD_SET_FDEV     17
#define CMD_GET_ENC      18
#define CMD_SET_ENC      19

#define CMD_LOAD_FRAME   20
#define CMD_APPEND_FRAME 21

#define CMD_RF_SEND      51

// Response frame bytes
#define RES_ID      0
#define RES_STATUS  1
#define RES_PAYLOAD 2

// Response status codes
#define STATUS_OK                   0
#define STATUS_RESERVED             STUFF_BYTE1
#define STATUS_UNKNOWN_CMD          0x01
#define STATUS_LOGIC_ERROR          0x02
#define STATUS_INVALID_FRAME_LEN    0x10
#define STATUS_INVALID_ARGUMENT     0x11
#define STATUS_INVALID_SEND_COOKIE  0x50
#define STATUS_TOO_MUCH_DATA        0x51

#endif // __SERCO_DEFINES_H__
