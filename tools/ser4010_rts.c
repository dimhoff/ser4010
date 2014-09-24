/**
 * ser4010_rts.c - Somfy RTS implementation for SER4010 RF sender
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
#include "serco.h"
#include "ser4010.h"

#include <stdbool.h>

#define wRts_BitRate_c	(2416)	// Rate at which bits are serialized, Rts = 604 us
					// Bit width in seconds = (bit_rate*(ods_ck_div+1))/24MHz
#define bRts_GroupWidth_c	(7)	// Amount of bits minus 1 encoded per byte in frame array
#define bRts_MaxFrameSize_c	(23)	// length of frame buffer in bytes
#define bRts_PreambleSize_c	(9)	// offset of payload in frame buffer in bytes
// Array which holds the frame bits
// WARNING: LSB shifted out first!!!!!
uint8_t abRts_FrameArray[bRts_MaxFrameSize_c] = {
		0x80, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, // hardware sync
		0x7F, // software sync
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // payload
};

/**
 * Manchester encode data
 *
 * @param enc_data	pointer to the next byte to write in the transmit data
 *			buffer
 * @param b		Byte to encode
 *
 * @returns	Updated pointer to the next byte to write in the transmit data
 *		buffer
 */
static uint8_t *encode_rts(uint8_t *enc_data, uint8_t b) 
{
	uint8_t i;

	i=8;
	while (i > 0) {
		*enc_data = (*enc_data >> 2) & 0x3f;
		if (b & 0x80) {
			*enc_data |= 0x80;
		} else {
			*enc_data |= 0x40;
		}
		b <<= 1;

		if (i == 5) {
			enc_data++;
		}
		i--;
	}

	return (enc_data+1);
}

int ser4010_rts_init(struct serco *sdev)
{
	int ret;
	tOds_Setup rOdsSetup;
	tPa_Setup rPaSetup;
	float fFreq;

	// Setup the PA.
	// Zero out Alpha and Beta here. They have to do with the antenna.
	// Chose a nice high PA Level. This value, along with the nominal cap
	// come from the CAL SPREADSHEET 
	rPaSetup.fAlpha      = 0;
	rPaSetup.fBeta       = 0;
	rPaSetup.bLevel      = 60;
	rPaSetup.wNominalCap = 256;
	rPaSetup.bMaxDrv     = 0;

	// Setup the ODS 
	rOdsSetup.bModulationType = 0;  // Use OOK
	rOdsSetup.bClkDiv         = 5;
	rOdsSetup.bEdgeRate       = 0;
	rOdsSetup.bGroupWidth     = bRts_GroupWidth_c;
	rOdsSetup.wBitRate        = wRts_BitRate_c;	// Bit width in seconds = (ods_datarate*(ods_ck_div+1))/24MHz
	rOdsSetup.bLcWarmInt      = 8;
	rOdsSetup.bDivWarmInt     = 5;
	rOdsSetup.bPaWarmInt      = 4;

	fFreq = 433.46e6;

	ret = ser4010_set_ods(sdev, &rOdsSetup);
	if (ret != STATUS_OK) {
		return ret;
	}

	ret = ser4010_set_pa(sdev, &rPaSetup);
	if (ret != STATUS_OK) {
		return ret;
	}

	ret = ser4010_set_freq(sdev, fFreq);
	if (ret != STATUS_OK) {
		return ret;
	}

	ret = ser4010_reconfigure(sdev);
	if (ret != STATUS_OK) {
		return ret;
	}

	return STATUS_OK;
}

int ser4010_rts_send(struct serco *sdev, uint8_t data[7], bool long_press)
{
	int ret;
  	uint8_t *pbFrameHead;		// Pointer to frame Head 
	int frame_cnt;
	int i;

	if (long_press) {
		frame_cnt = 200;
	} else {
		frame_cnt = 4;
	}

	pbFrameHead = &abRts_FrameArray[bRts_PreambleSize_c];
	for (i = 0; i < 7; i++) {
		pbFrameHead = encode_rts(pbFrameHead, data[i]);
	}

	ret = ser4010_load_frame(sdev, abRts_FrameArray, bRts_MaxFrameSize_c);
	if (ret != STATUS_OK) {
		return ret;
	}

	ret = ser4010_send(sdev, frame_cnt);
	if (ret != STATUS_OK) {
		return ret;
	}

	return STATUS_OK;
}
