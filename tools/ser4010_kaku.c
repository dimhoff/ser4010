/**
 * ser4010_kaku.c - Klik Aan-Klik Uit implementation for SER4010 RF sender
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "serco.h"
#include "ser4010.h"

#define wKaku_BitRate_c		(1100)	// Rate at which bits are serialized, KaKu = 275 us
					// Bit width in seconds = (bit_rate*(ods_ck_div+1))/24MHz
#define bKaku_GroupWidth_c	(6)	// Amount of bits minus 1 encoded per byte in frame array
					// One Kaku symbols encode to 7 bits.
#define bKaku_MaxFrameSize_c	(35+4)	// length of frame buffer in bytes
#define bKaku_PreambleSize_c	(2)	// offset of payload in frame buffer in bytes
// Array which holds the frame bits
// WARNING: LSB shifted out first!!!!!
uint8_t abKaku_FrameArray[bKaku_MaxFrameSize_c] = {
		0x20, 0x00, // Start Bit
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, // Stop bit ( actually needs 32-6 more symbol times...)

//TODO: temporary added the 32-6 extra symbol inter frame gap, but should be 
//done with timer so that we don't wast energy of keeping the transmitter enabled.
		0x00, 0x00, 0x00, 0x00
};

#define KAKU_MARK 0x21
#define KAKU_SPACE 0x05

/**
 * Perform KAKU PWM encoding on data
 *
 * Takes one byte and encodes it using the KAKU PWM encoding. One bit is
 * encoded into one byte. Only 7 bits in the output bytes are used, so
 * the SI4010 bGroupWidth must be 6.
 *
 * @param enc_data	pointer to the next byte to write in the transmit data
 *			buffer
 * @param b		Byte to encode
 *
 * @returns	Updated pointer to the next byte to write in the transmit data
 *		buffer
 */
static uint8_t *encode_kaku(uint8_t *enc_data, uint8_t b) 
{
	//NOTE: expects bGroupWidth to be set to 6 (eg. 7 symbols per byte)
	int i;

	i=8;
	while (i > 0) {
		if (b & 0x80) {
			*enc_data = KAKU_MARK;
		} else {
			*enc_data = KAKU_SPACE;
		}
		b <<= 1;

		enc_data++;
		i--;
	}

	return enc_data;
}

/**
 * Init RF module for KAKU usage
 *
 * Set up the SI4010 for sending KAKU frames. This must be called
 * before ser4010_kaku_send() can be used.
 *
 * @param sdev		Serial Communication handle
 */
int ser4010_kaku_init(struct serco *sdev)
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
	rOdsSetup.bGroupWidth     = bKaku_GroupWidth_c;
	rOdsSetup.wBitRate        = wKaku_BitRate_c;	// Bit width in seconds = (ods_datarate*(ods_ck_div+1))/24MHz
	rOdsSetup.bLcWarmInt      = 8;
	rOdsSetup.bDivWarmInt     = 5;
	rOdsSetup.bPaWarmInt      = 4;

	fFreq = 433.9e6;

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

	return STATUS_OK;
}

/**
 * Send a frame using KAKU
 *
 * This function encodes the data in 'payload' according to the KAKU protocol,
 * pre-/appends the preamble/Inter-frame gap, and sends out the frame 4 times
 * using OOK modulation on 433.9 MHz.
 *
 * @param sdev		Serial Communication handle
 * @param data		The 4-bytes frame data
 */
int ser4010_kaku_send(struct serco *sdev, uint8_t data[4])
{
	int ret;
  	uint8_t *pbFrameHead;		// Pointer to frame Head 

	pbFrameHead = &abKaku_FrameArray[bKaku_PreambleSize_c];
	pbFrameHead = encode_kaku(pbFrameHead, data[0]);
	pbFrameHead = encode_kaku(pbFrameHead, data[1]);
	pbFrameHead = encode_kaku(pbFrameHead, data[2]);
	pbFrameHead = encode_kaku(pbFrameHead, data[3]);

	ret = ser4010_load_frame(sdev, abKaku_FrameArray, bKaku_MaxFrameSize_c);
	if (ret != STATUS_OK) {
		return ret;
	}

	ret = ser4010_send(sdev, 4);
	if (ret != STATUS_OK) {
		return ret;
	}

	return STATUS_OK;
}

void usage(const char *name)
{
	fprintf(stderr,
		"usage: %s [options] <address> <unit> <on|off>\n"
		"\n"
		"Options:\n"
		" -d <path>	Path to serial device file\n"
		" -h		Print this help message\n"
		"\n"
		"Arguments:\n"
		"address: The hexadecimal address of the remote\n"
		"unit: The unit number(0-15) of a multi channel remote\n"
		"on|off: the action to perform\n"
		, name);
}

int main(int argc, char *argv[])
{
	int opt;
	char *dev_path;
	struct serco sdev;
	unsigned char kaku_data[4];
	enum { ButtonOn, ButtonOff } button;
	char *tmp;
	uint32_t addr;
	int unit;
	int ret;

	dev_path = strdup(DEFAULT_SERIAL_DEV);

	while ((opt = getopt(argc, argv, "d:h")) != -1) {
		switch (opt) {
		case 'd':
			free(dev_path);
			dev_path = strdup(optarg);
			break;
		case 'h':
			usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		default: /* '?' */
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	
	if (argc - optind != 3) {
		fprintf(stderr, "Incorrect amount of arguments\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	addr = strtol(argv[optind], &tmp, 16);
	if (*tmp != '\0') {
		fprintf(stderr, "Unparsable characters in address argument\n");
		exit(EXIT_FAILURE);
	}
	optind++;

	unit = strtol(argv[optind], &tmp, 0);
	if (*tmp != '\0') {
		fprintf(stderr, "Unparsable characters in unit argument\n");
		exit(EXIT_FAILURE);
	}
	if (unit > 0xf || unit < 0) {
		fprintf(stderr, "Unit number out of range(0-15)\n");
		exit(EXIT_FAILURE);
	}
	optind++;

	if (strcmp(argv[optind], "on") == 0) {
		button = ButtonOn;
	} else if (strcmp(argv[optind], "off") == 0) {
		button = ButtonOff;
	} else {
		fprintf(stderr, "Unknown direction argument\n");
		exit(EXIT_FAILURE);
	}

	// open/init SER4010
	if (serco_open(&sdev, dev_path) != 0) {
		exit(EXIT_FAILURE);
	}

	ret = ser4010_kaku_init(&sdev);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		} else if (ret == -1) {
			perror("Failed configuring module");
		} else {
			fprintf(stderr, "Failed configuring module: %d\n", ret);
		}
		serco_close(&sdev);
		exit(EXIT_FAILURE);
	}

	// encode KAKU frame data
	kaku_data[0] = addr >> 18;
	kaku_data[1] = addr >> 10;
	kaku_data[2] = addr >> 2;
	kaku_data[3] = (addr << 6) & 0xc0;
	if (button == ButtonOn) {
		kaku_data[3] |= 0x10;
	} else {
		kaku_data[3] &= ~0x10;
	}
	kaku_data[3] = (kaku_data[3] & 0xF0) | (unit & 0x0F);

	// send frame
	ret = ser4010_kaku_send(&sdev, kaku_data);

	serco_close(&sdev);

	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		} else if (ret == -1) {
			perror("Failed sending command");
		} else {
			fprintf(stderr, "Failed sending command: %d\n", ret);
		}
		exit(EXIT_FAILURE);
	}

	return 0;
}
