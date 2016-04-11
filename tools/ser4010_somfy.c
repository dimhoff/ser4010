/**
 * ser4010_somfy.c - Control Somfy RTS blinds through SI4010 radio PHY with
 *                    SER4010 firmware
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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>

#include "serco.h"
#include "ser4010_rts.h"

/**
 * Send long button press or normal button press
 */
bool long_press = false;

typedef enum {
	CONTROL_MY	= 0x1,
	CONTROL_UP	= 0x2,
	CONTROL_DOWN	= 0x4,
	CONTROL_PROG	= 0x8,
} somfy_control_t;

void usage(char *my_name) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s [Options..] -r raw_frame\n", my_name);
	fprintf(stderr, "\t%s [Options..] up|down|my|prog key address sequence\n", my_name);
	fprintf(stderr, "\t%s [Options..] up|down|my|prog state_file\n", my_name);
	fprintf(stderr, "\n"
			"Options:\n"
			" -d <path>  Serial device path\n"
			" -l         Generate long button press\n"
			" -h         Display this help message\n"
		);
	fprintf(stderr, "\n"
			"'key' is a fixed length hexadecimal strings of 2 characters (eg. 01).\n"
			"'address' is a fixed length hexadecimal strings of 6 characters (eg. 001122).\n"
			"'sequence' is a number with optionally base prefixed (eg. 123 or 0xfb).\n");
	fprintf(stderr, "\n"
			"State files contain a single line with the following format:\n"
			"    KK AAAAAA RRRR\n"
			"Where:\n"
			" KK = Key byte in Hexadecimal.\n"
			" AAAAAA = Address in Hexadecimal.\n"
			" RRRR = Rolling code in Hexadecimal.\n"
			"State files are automatically updated to the next sequence and key after use.\n");

}

/**
 * Read state file
 *
 * Read the current state from file. The file should contain a single line in
 * the following format, without leading or trailing space:
 *
 *     KK AAAAAA RRRR
 *
 * Where:
 * KK = Key byte in Hexadecimal
 * AAAAAA = Address in Hexadecimal
 * RRRR = Rolling code in Hexadecimal
 *
 * @param path	Path to state file
 * @param key	Pointer to variable to return key in
 * @param addr	Pointer to variable to return address in
 * @param seq	Pointer to variable to return rolling code in
 *
 * @return	0 on success, -1 on failure
 **/
int read_state_file(const char *path, uint8_t *key, uint32_t *addr, uint16_t *seq)
{
	FILE *fp;
	char buf[100];
	ssize_t bytes_read;
	char *sp;
	int retval = -1;

	if ((fp = fopen(path, "r")) == NULL) {
		perror("Failed to open state file for reading");
		return -1;
	}

	bytes_read = fread(buf, 1, sizeof(buf), fp);
	if (bytes_read != 15 || buf[2] != ' ' || buf[9] != ' ' || buf[14] != '\n') {
		fprintf(stderr, "state file illegal format\n");
		goto bad;
	}

	*key = strtol(buf, &sp, 16);
	if (sp != &(buf[2])) {
		fprintf(stderr, "illegal key format\n");
		goto bad;
	}
	sp++;

	*addr = strtol(sp, &sp, 16);
	if (sp != &(buf[9])) {
		fprintf(stderr, "illegal address format\n");
		goto bad;
	}
	sp++;

	*seq = strtol(sp, &sp, 16);
	if (sp != &(buf[14])) {
		fprintf(stderr, "illegal sequence format\n");
		goto bad;
	}

	retval = 0;
bad:
	fclose(fp);
	return retval;
}

int write_state_file(const char *path, uint8_t key, uint32_t addr, uint16_t seq)
{
	FILE *fp;
	int retval = -1;

	if ((fp = fopen(path, "w")) == NULL) {
		perror("Failed to open state file for writing");
		return -1;
	}

	if (fprintf(fp, "%.2x %.6x %.4x\n", key, addr, seq) < 0) {
		perror("Failed to write state file");
		goto bad;
	}

	retval = 0;
bad:
	fclose(fp);
	return retval;
}

uint8_t somfy_calc_checksum(uint8_t frame[7])
{
	int checksum=0;
	int i;

	for (i=0; i < 7; i++) {
		checksum ^= frame[i] & 0xf;
		checksum ^= (frame[i] >> 4) & 0xf;
	}

	return (checksum & 0xf);
}

int send_somfy_raw(struct serco *dev, uint8_t data[7])
{
#ifdef SOMFY_DEBUG
	int i;
	for (i=0; i< sizeof(data); i++) {
		printf("%.2x ", data[i]);
	}
	putchar('\n');
#endif

	return ser4010_rts_send(dev, data, long_press);
}

int send_somfy_command(struct serco *dev, uint8_t key, uint32_t addr, uint16_t seq, somfy_control_t ctrl)
{
	unsigned char frame[7];
	int i;

	frame[0] = 0xa0 | (key & 0xf);
	switch (ctrl) {
	case CONTROL_MY:
		frame[1] = 1 << 4;
		break;
	case CONTROL_UP:
		frame[1] = 2 << 4;
		break;
	case CONTROL_DOWN:
		frame[1] = 4 << 4;
		break;
	case CONTROL_PROG:
		frame[1] = 8 << 4;
		break;
	default:
		assert(0);
	}
	frame[2] = (seq & 0xFF00) >> 8;
	frame[3] = seq & 0xFF;
	frame[4] = addr & 0xFF;
	frame[5] = (addr & 0xFF00) >> 8;
	frame[6] = (addr & 0xFF0000) >> 16;

	// calculate checksum
	frame[1] |= somfy_calc_checksum(frame);

	// encrypt
	for (i=1; i < 7; i++) {
		frame[i] = frame[i] ^ frame[i-1];
	}

	return send_somfy_raw(dev, frame);
}

int dehex_nibble(int n)
{
	int x;
	x = n - 0x30;
	if (x > 9)
		x -= 0x07;
	if (x > 15)
		x -= 0x20;

	return x;
}

int dehexify(char *in, size_t bytes, unsigned char *out)
{
	int i;
	int x;

	if (strlen(in) < bytes*2) {
		return -1;
	}

	memset(out, 0, bytes);
	for (i=0; i<bytes; i++) {
		x = dehex_nibble(in[(i*2)]);
		if (x < 0 || x > 15)
			return -1;
		out[i] = (x << 4);

		x = dehex_nibble(in[(i*2)+1]);
		if (x < 0 || x > 15)
			return -1;
		out[i] |= x;

		printf("%d - %x\n", i, out[i]);
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct serco dev;
	int ret;

	int opt;
	enum { RAW, NORMAL } mode = NORMAL;
	char *dev_path = NULL;
	char *state_file_path = NULL;

	somfy_control_t ctrl = CONTROL_MY;
	uint8_t key;
	uint32_t addr;
	uint16_t seq;

	uint8_t raw_data[7];

	int retval = 1;

	dev_path = DEFAULT_SERIAL_DEV;

	while ((opt = getopt(argc, argv, "rld:h")) != -1) {
		switch (opt) {
		case 'r':
			mode = RAW;
			break;
		case 'l':
			long_press = true;
			break;
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

	if (mode == RAW) {
		if (argc - optind != 1) {
			usage(argv[0]);
			exit(1);
		}
		if (strlen(argv[optind]) != 14) {
			fprintf(stderr, "data must be a hexadecimal 56-bit/7 byte(eg. 11223344556677)\n");
			exit(1);
		}

		if (dehexify(argv[optind], 7, raw_data) != 0) {
			fprintf(stderr, "data must be a hexadecimal 56-bit/7 byte(eg. 11223344556677)\n");
			exit(1);
		}

	} else {
		char *sp;

		if (argc - optind != 2 && argc - optind != 4) {
			usage(argv[0]);
			exit(1);
		}

		if (strcmp(argv[optind], "down") == 0) {
			ctrl = CONTROL_DOWN;
		} else if (strcmp(argv[optind], "up") == 0) {
			ctrl = CONTROL_UP;
		} else if (strcmp(argv[optind], "my") == 0) {
			ctrl = CONTROL_MY;
		} else if (strcmp(argv[optind], "prog") == 0) {
			ctrl = CONTROL_PROG;
		} else {
			fprintf(stderr, "illegal control name\n");
			usage(argv[0]);
			exit(1);
		}
		optind++;

		if (argc - optind == 1) {
			state_file_path = strdup(argv[optind]);
			optind++;
		} else {
			key = strtol(argv[optind], &sp, 16);
			if (*sp != '\0' || strlen(argv[optind]) != 2) {
				fprintf(stderr, "key must be a hexadecimal 1 byte string(eg. 01)\n");
				usage(argv[0]);
				exit(1);
			}
			optind++;

			addr = strtol(argv[optind], &sp, 16);
			if (*sp != '\0' || strlen(argv[optind]) != 6) {
				fprintf(stderr, "address must be a hexadecimal 3 byte string(eg. 001122)\n");
				usage(argv[0]);
				exit(1);
			}
			optind++;

			seq = strtol(argv[optind], &sp, 0);
			if (*sp != '\0') {
				fprintf(stderr, "illegal sequence format\n");
				usage(argv[0]);
				exit(1);
			}
			optind++;
		}
	}

	if (state_file_path != NULL) {
		if (read_state_file(state_file_path, &key, &addr, &seq) != 0) {
			goto bad1;
		}
	}

	if (serco_open(&dev, dev_path) != 0) {
		goto bad1;
	}

	ret = ser4010_rts_init(&dev);
	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		} else if (ret == -1) {
			perror("Failed configuring module");
		} else {
			fprintf(stderr, "Failed configuring module: %d\n", ret);
		}
		serco_close(&dev);
		goto bad1;
	}

	if (mode == RAW) {
		ret = send_somfy_raw(&dev, raw_data);
	} else {
		ret = send_somfy_command(&dev, key, addr, seq, ctrl);
	}

	serco_close(&dev);

	if (ret != STATUS_OK) {
		if (ret > 0) {
			fprintf(stderr, "Result status indicates error 0x%.2x\n", ret);
		} else if (ret == -1) {
			perror("Failed sending command");
		} else {
			fprintf(stderr, "Failed sending command: %d\n", ret);
		}
		goto bad1;
	}

	if (state_file_path != NULL) {
		key = 0xa0 | ((key + 1) & 0xf);
		seq++;
		if (write_state_file(state_file_path, key, addr, seq) != 0) {
			goto bad1;
		}
	}

	retval = 0;

bad1:
	if (state_file_path != NULL) {
		free(state_file_path);
	}

	return retval;
}

