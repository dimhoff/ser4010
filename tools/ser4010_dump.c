/**
 * ser4010_dump.c - Dump configuration of SER4010 device
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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <endian.h>

#include "serco.h"
#include "ser4010.h"

void usage(const char *name)
{
	fprintf(stderr,
		"Usage: %s [Options..]\n"
		"\n"
		"Options:\n"
		" -d <path>	Path to serial device file\n"
		" -h		Print this help message\n"
		, name);
}

const char *modulation_type_to_str(int type)
{
	static const char *strings[3] = {
		"OOK",
		"FSK",
		"Invalid"
	};
	if (type > 1 || type < 0) type = 2;
	return strings[type];
}

const char *encoding_to_str(int enc)
{
	static const char *strings[4] = {
		"None/NRZ",
		"Manchester",
		"4b5b",
		"Invalid"
	};
	if (enc > 2 || enc < 0) enc = 3;
	return strings[enc];
}

int main(int argc, char *argv[])
{
	int opt;
	char *dev_path;
	struct serco sdev;
	int ret;
	int retval = EXIT_SUCCESS;

	uint16_t dev_type;
	uint16_t dev_rev;
	tOds_Setup ods_data;
	tPa_Setup pa_data;
	enum Ser4010Encoding enc;
	float freq;
	uint8_t fdev;

	dev_path = DEFAULT_SERIAL_DEV;

	while ((opt = getopt(argc, argv, "d:h")) != -1) {
		switch (opt) {
		case 'd':
			dev_path = optarg;
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
	
	if (argc - optind != 0) {
		fprintf(stderr, "Incorrect amount of arguments\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (serco_open(&sdev, dev_path) != 0) {
		exit(EXIT_FAILURE);
	}

	ret = ser4010_get_dev_type(&sdev, &dev_type);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "ser4010_get_dev_type(): "
				"Result status indicates error 0x%.2x\n",
				ret);
		}
		retval = EXIT_FAILURE;
		goto bad;
	}

	ret = ser4010_get_dev_rev(&sdev, &dev_rev);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "ser4010_get_dev_rev(): "
				"Result status indicates error 0x%.2x\n",
				ret);
		}
		retval = EXIT_FAILURE;
		goto bad;
	}

	ret = ser4010_get_ods(&sdev, &ods_data);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		}
		retval = EXIT_FAILURE;
		goto bad;
	}

	ret = ser4010_get_pa(&sdev, &pa_data);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		}
		retval = EXIT_FAILURE;
		goto bad;
	}

	ret = ser4010_get_enc(&sdev, &enc);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "ser4010_get_enc(): "
				"Result status indicates error 0x%.2x\n", ret);
		}
		retval = EXIT_FAILURE;
		goto bad;
	}

	ret = ser4010_get_freq(&sdev, &freq);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		}
		retval = EXIT_FAILURE;
		goto bad;
	}

	ret = ser4010_get_fdev(&sdev, &fdev);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		}
		retval = EXIT_FAILURE;
		goto bad;
	}

	printf("Device Info:\n");
	printf("------------\n");
	printf("Device Type: 0x%04hx\n", dev_type);
	printf("Device Revision: %hu\n", dev_rev);
	printf("\n");
	printf("ODS settings:\n");
	printf("------------\n");
	printf("bModulationType: %s (%u)\n", modulation_type_to_str(ods_data.bModulationType), ods_data.bModulationType);
	printf("bClkDiv: %u\n", ods_data.bClkDiv);
	printf("bEdgeRate: %u\n", ods_data.bEdgeRate);
	printf("bGroupWidth: %u\n", ods_data.bGroupWidth);
	printf("wBitRate: %u\n", ods_data.wBitRate);
	printf("bLcWarmInt: %u\n", ods_data.bLcWarmInt);
	printf("bDivWarmInt: %u\n", ods_data.bDivWarmInt);
	printf("bPaWarmInt: %u\n", ods_data.bPaWarmInt);
	printf("\n");
	printf("PA settings:\n");
	printf("------------\n");
	printf("fAlpha: %f\n", pa_data.fAlpha);
	printf("fBeta: %f\n", pa_data.fBeta);
	printf("bLevel: %u\n", pa_data.bLevel);
	printf("bMaxDrv: %u\n", pa_data.bMaxDrv);
	printf("wNominalCap: %u\n", pa_data.wNominalCap);
	printf("\n");
	printf("Encoder settings:\n");
	printf("------------\n");
	printf("Encoding: %s (%u)\n", encoding_to_str(enc), enc);
	printf("\n");
	printf("Freq settings:\n");
	printf("------------\n");
	printf("frequency: %f\n", freq);
	printf("freq. deviation: %u\n", fdev);
bad:
	serco_close(&sdev);
	return retval;
}
