/**
 * ser4010_pulse.c - Send test pulse
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

void usage(const char *name)
{
	fprintf(stderr,
		"usage: %s [options]\n"
		"\n"
		"Options:\n"
		" -d <path>	Path to serial device file\n"
		" -f <freq>     Frequency in Hz\n"
		" -t <usec>     Pulse length in microseconds\n"
		" -A <fAlpha>   PA Alpha value\n"
		" -B <fBeta>    PA Beta value\n"
		" -L <bLevel>   PA power level\n"
		" -C <cap>      PA Nominal Capacitance\n"
		" -M            Enable PA Max. Drive\n"
		" -h		Print this help message\n"
		"\n"
		"WARNING: Only use for testing purposes in a controlled environment.\n"
		, name);
}

int main(int argc, char *argv[])
{
	int ret;
	int opt;
	unsigned long opt_ulong;

	char *dev_path;
	tOds_Setup rOdsSetup;
	tPa_Setup rPaSetup;
	float fFreq;

	struct serco sdev;

	// Array which holds the frame bits
	// WARNING: LSB shifted out first!!!!!
	uint8_t frame_buf[1] = { 0xff };

	// Default the PA values.
	rPaSetup.fAlpha      = 0;
	rPaSetup.fBeta       = 0;
	rPaSetup.bLevel      = 60;
	rPaSetup.wNominalCap = 512 / 2; // = 50 % of range
	rPaSetup.bMaxDrv     = 0;

	// Default the ODS values.
	rOdsSetup.bModulationType = 0;  // Use OOK
	rOdsSetup.bClkDiv         = 8-1; // 24 MHz/8 = 3 MHz
	rOdsSetup.bEdgeRate       = 0;
	rOdsSetup.bGroupWidth     = 8-1;
	rOdsSetup.wBitRate        = 0x7FFF; // Bit width in seconds = (ods_datarate*(ods_ck_div+1))/24MHz. Max. 0x7FFF => 10.92 ms per bit
	rOdsSetup.bLcWarmInt      = 0xf; //8;
	rOdsSetup.bDivWarmInt     = 0xf; //5;
	rOdsSetup.bPaWarmInt      = 0xf; //4;

	// Default Frequency
	fFreq = 433.92e6;

	// Default device path
	dev_path = strdup(DEFAULT_SERIAL_DEV);

	while ((opt = getopt(argc, argv, "d:f:A:B:L:C:Mt:h")) != -1) {
		switch (opt) {
		case 'd':
			free(dev_path);
			dev_path = strdup(optarg);
			break;
		case 'f':
			fFreq = strtof(optarg, NULL);
			if (fFreq < 27 * 1000 * 1000 || fFreq >= 960 * 1000 * 1000) {
				fprintf(stderr, "Frequency out of range\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'A':
			rPaSetup.fAlpha = strtof(optarg, NULL);
			/*
			if (rPaSetup.fAlpha < 0) {
				fprintf(stderr, "Alpha out of range\n");
				exit(EXIT_FAILURE);
			}*/
			break;
		case 'B':
			rPaSetup.fBeta = strtof(optarg, NULL);
			/*
			if (rPaSetup.fBeta < 0) {
				fprintf(stderr, "Beta out of range\n");
				exit(EXIT_FAILURE);
			}*/
			break;
		case 'L':
			opt_ulong = strtoul(optarg, NULL, 0);
			if (opt_ulong > 0x7f) {
				fprintf(stderr, "PA Power Level out of range\n");
				exit(EXIT_FAILURE);
			}
			rPaSetup.bLevel = opt_ulong;
			break;
		case 'C':
			opt_ulong = strtoul(optarg, NULL, 0);
			if (opt_ulong >= 0x200) {
				fprintf(stderr, "PA Nominal Capacitance out of range\n");
				exit(EXIT_FAILURE);
			}
			rPaSetup.wNominalCap = opt_ulong;
			break;
		case 'M':
			rPaSetup.bMaxDrv = 1;
			break;
		case 't':
			fprintf(stderr, "TODO:\n");
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

	// open/init SER4010
	if (serco_open(&sdev, dev_path) != 0) {
		exit(EXIT_FAILURE);
	}

	ret = ser4010_set_ods(&sdev, &rOdsSetup);
	if (ret != STATUS_OK) {
		fprintf(stderr, "ser4010_set_ods() Failed: %d", ret);
		exit(EXIT_FAILURE);
	}

	ret = ser4010_set_pa(&sdev, &rPaSetup);
	if (ret != STATUS_OK) {
		fprintf(stderr, "ser4010_set_pa() Failed: %d", ret);
		exit(EXIT_FAILURE);
	}

	ret = ser4010_set_freq(&sdev, fFreq);
	if (ret != STATUS_OK) {
		fprintf(stderr, "ser4010_set_freq() Failed: %d", ret);
		exit(EXIT_FAILURE);
	}

	ret = ser4010_load_frame(&sdev, frame_buf, sizeof(frame_buf));
	if (ret != STATUS_OK) {
		fprintf(stderr, "ser4010_load_frame() Failed: %d", ret);
		exit(EXIT_FAILURE);
	}

	ret = ser4010_send(&sdev, 1);
	if (ret != STATUS_OK) {
		fprintf(stderr, "ser4010_send() Failed: %d", ret);
		exit(EXIT_FAILURE);
	}

	serco_close(&sdev);

	return 0;
}
