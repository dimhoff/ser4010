/**
 * ser4010_smartwares_db.c - Smartwares B & DB compatible code sender
 *
 * Copyright (c) 2018, David Imhoff <dimhoff.devel@gmail.com>
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

#define SWARES_DB_BITRATE		500	// Symbol rate of protocol
#define SWARES_DB_BITS_PER_FRAME	32	// Amount of data bits in one frame
#define SWARES_DB_PREAMBLE_LEN		4	// Amount of bit preamble
#define SWARES_DB_FRAME_REPEAT		20	// Amount of times frame is send on 1 button press

#define SWARES_DB_GROUPWIDTH 4		// In how many bits a symbol is encoded for the ODS
#define SWARES_DB_ODS_RATE (SWARES_DB_BITRATE * SWARES_DB_GROUPWIDTH / 1000)
#define ENCODED_MARK  0x07		// Encoded Mark '1'  WARNING: LSB shifted out first!!!!!
#define ENCODED_SPACE 0x01		// Encoded Space '0' WARNING: LSB shifted out first!!!!!

/**
 * Encode Byron Smartwares B & DB compatible frame for ODS
 *
 * Encode 32-bit Byron Smartwares B & DB compatible frame data into a encoded
 * buffer for sending with Si4010 ODS. Groupwidth is 4 bit(=bGroupWidth = 4-1).
 *
 * @param frame	Data to encode
 * @param buf	Output buffer. Must be able to hold SWARES_DB_BITS_PER_FRAME +
 *		SWARES_DB_PREAMBLE_LEN bytes.
 */
static void encode_swares_db(uint32_t frame, uint8_t *buf) 
{
	uint32_t mask = 0x80000000;

	// Encode Preamble
	*buf = 0x1; buf++;
	*buf = 0x0; buf++;
	*buf = 0x0; buf++;
	*buf = 0x0; buf++;

	// NOTE: This has actually 1 symbol to much between the start bit and
	// the data. This is because the preamble is 15 symbol lengths long.
	// While bits are encoded in 4 symbol lengths, we only send multiples
	// of 4.

	while (mask != 0) {
		if (frame & mask) {
			*buf = ENCODED_MARK;
		} else {
			*buf = ENCODED_SPACE;
		}

		buf++;
		mask >>= 1;
	}
}

/**
 * Init RF module for Smartwares usage
 *
 * Set up the SI4010 for sending Smartwares frames. This must be called
 * before ser4010_smartwares_send() can be used.
 *
 * @param sdev		Serial Communication handle
 */
int ser4010_smartwares_init(struct serco *sdev)
{
	int ret;
	tPa_Setup rPaSetup;

	// Setup the PA.
	rPaSetup.fAlpha      = 0;	// Disable radiate power adjustment
	rPaSetup.fBeta       = 0;
	rPaSetup.bLevel      = 60;	// default...
	rPaSetup.bMaxDrv     = 0;
	rPaSetup.wNominalCap = 256;	// = half way the range

	ret = ser4010_config(sdev, 433.92, 0, ODS_MODULATION_TYPE_OOK, bEnc_NoneNrz_c, SWARES_DB_ODS_RATE, SWARES_DB_GROUPWIDTH);
	if (ret != STATUS_OK) {
		return ret;
	}

	ret = ser4010_set_pa(sdev, &rPaSetup);
	if (ret != STATUS_OK) {
		return ret;
	}

	return STATUS_OK;
}

/**
 * Send a Byron Smartwares B & DB compatible frame
 *
 * @param sdev		Serial Communication handle
 * @param data		The 32-bit frame data
 */
int ser4010_swares_db_send(struct serco *sdev, uint32_t data)
{
	int ret;
	uint8_t frame[SWARES_DB_PREAMBLE_LEN + SWARES_DB_BITS_PER_FRAME] = { 0x00 };

	encode_swares_db(data, frame);

	ret = ser4010_load_frame(sdev, frame, sizeof(frame));
	if (ret != STATUS_OK) {
		return ret;
	}

	ret = ser4010_send(sdev, SWARES_DB_FRAME_REPEAT);
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
		"data: The hexadecimal data of frame\n"
		, name);
}

int main(int argc, char *argv[])
{
	int opt;
	char *dev_path;
	struct serco sdev;
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
	
	if (argc - optind != 1) {
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

	// open/init SER4010
	if (serco_open(&sdev, dev_path) != 0) {
		exit(EXIT_FAILURE);
	}

	ret = ser4010_swares_db_init(&sdev);
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

	// send frame
	ret = ser4010_swares_db_send(&sdev, addr);

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
