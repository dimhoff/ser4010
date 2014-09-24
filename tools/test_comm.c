/**
 * test_comm.c - Test if serco serial communication is working
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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "serco.h"

#define DEVICE "/dev/ttyUSB0"

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

int main(int argc, char *argv[])
{
	int opt;
	char *dev_path;
	struct serco sdev;
	int ret;

	dev_path = strdup(DEVICE);

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
	
	if (argc - optind != 0) {
		fprintf(stderr, "Incorrect amount of arguments\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (serco_open(&sdev, dev_path) != 0) {
		exit(EXIT_FAILURE);
	}

	ret = serco_send_command(&sdev, CMD_NOP, NULL, 0, NULL, 0);

	serco_close(&sdev);

	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		}
		exit(EXIT_FAILURE);
	}

	return 0;
}
