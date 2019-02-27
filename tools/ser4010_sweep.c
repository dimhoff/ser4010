/**
 * ser4010_sweep.c - Sweep through spectrum by sending small pulses
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

void usage(const char *name)
{
	fprintf(stderr,
		"usage: %s [options] <start_hz> <end_hz> <step>\n"
		"\n"
		"Options:\n"
		" -d <path>	Path to serial device file\n"
		" -h		Print this help message\n"
		"\n"
		"WARNING: Only use for testing purposes in a controlled environment.\n"
		, name);
}

int main(int argc, char *argv[])
{
	int ret;
	int opt;

	char *dev_path;
	float freq;
	float end_freq;
	float step;

	struct serco sdev;

	// Array which holds the frame bits
	// WARNING: LSB shifted out first!!!!!
	uint8_t frame_buf[] = { 0xff };
//	uint8_t frame_buf[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	// Default device path
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
		fprintf(stderr, "Incorrect number of arguments\n");
		exit(EXIT_FAILURE);
	}

	freq = strtof(argv[optind], NULL);
	if (freq < 27 * 1000 * 1000 || freq >= 960 * 1000 * 1000) {
		fprintf(stderr, "Start frequency out of range\n");
		exit(EXIT_FAILURE);
	}
	end_freq = strtof(argv[optind + 1], NULL);
	if (end_freq < 27 * 1000 * 1000 || end_freq >= 960 * 1000 * 1000) {
		fprintf(stderr, "End frequency out of range\n");
		exit(EXIT_FAILURE);
	}
	step = strtof(argv[optind + 2], NULL);
	if (step < 0 || step > 960 * 1000 * 1000) {
		fprintf(stderr, "Frequency step out of range\n");
		exit(EXIT_FAILURE);
	}

	// open/init SER4010
	if (serco_open(&sdev, dev_path) != 0) {
		exit(EXIT_FAILURE);
	}


	ret = ser4010_load_frame(&sdev, frame_buf, sizeof(frame_buf));
	if (ret != STATUS_OK) {
		fprintf(stderr, "ser4010_load_frame() Failed: %d", ret);
		exit(EXIT_FAILURE);
	}

	while (freq < end_freq) {
		printf("Sending at %f\n", freq);
		ret = ser4010_set_freq(&sdev, freq);
		if (ret != STATUS_OK) {
			fprintf(stderr, "ser4010_set_freq() Failed: %d", ret);
			exit(EXIT_FAILURE);
		}

		ret = ser4010_send(&sdev, 1);
		if (ret != STATUS_OK) {
			fprintf(stderr, "ser4010_send() Failed: %d", ret);
			exit(EXIT_FAILURE);
		}

		freq += step;
	}

	serco_close(&sdev);

	return 0;
}
