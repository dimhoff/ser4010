/**
 * ser4010_somfy.h - Somfy RTS implementation for SER4010 RF sender
 *
 * Copyright (c) 2014, David Imhoff <dimhoff_devel@xs4all.nl>
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
#ifndef __SER4010_RTS_H__
#define __SER4010_RTS_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * Init RF module for Somfy RTS usage
 *
 * Set up the module for usage with the Somfy RTS module. This must be called
 * before ser4010_rts_send() can be used.
 *
 * @param sdev		Serial Communication handle
 */
int ser4010_rts_init(struct serco *sdev);

/**
 * Send a frame using RTS
 *
 * This function manchester encodes the data in 'payload', pre-/appends the
 * preamble/Inter-frame gap, and sends out the frame 4 times using OOK
 * modulation on 433.46 MHz. If 'long_press' is true the frame will be repeated
 * more often, as required to initiate programming mode of the receiver.
 *
 * @param sdev		Serial Communication handle
 * @param data		The 7-bytes frame data
 * @param long_press	If true frame is be repeated for a longer period of time
 */
int ser4010_rts_send(struct serco *sdev, uint8_t data[7], bool long_press);

#endif // __SER4010_RTS_H__
