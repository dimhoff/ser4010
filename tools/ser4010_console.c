/**
 * ser4010_console.c - Interactive program to control Ser4010 hardware
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
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "serco.h"
#include "ser4010.h"
#include "dehexify.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define UNUSED(x) (void)(x)

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

/**
 * Split string into arguments on spaces
 *
 * Splits a string into arguments. The original string will be modified by
 * overwriting found separators with '\0'. The start locations of the argument
 * strings are returned in the argv array. The 'argv' array is preallocated by
 * the caller and should be big enough to contain 'max_args' entries.
 *
 * Empty fields are ignored.
 *
 * Note this Currently doesn't support any quoting.
 *
 * @param cmd_str	String to split. This string is modified!
 * @param argv		Pointer to array of pointers to place pointers of found
 *                      arguments in.
 * @param max_args	Max. number of entries the 'argv' array can hold.
 *
 * @returns		Amount of arguments found. If equal to max_args then
 * 			the last argument contains the rest including spaces.
 */
size_t str_to_args(char *cmd_str, char **argv, size_t max_args)
{
	size_t args_found = 0;

	while (cmd_str[0] != '\0') {
		// Strip trailing spaces
		while (isspace(cmd_str[0])) {
			cmd_str++;
		}

		if (cmd_str[0] == '\0') break;

		// Add argument to argv
		argv[args_found] = cmd_str;
		args_found++;

		// if args_found == max_args, then don't terminate argument but
		// return remainder of cmd_str as last arg.
		if (args_found == max_args) break;

		// Find end of arg, and terminate with '\0'
		while (cmd_str[0] != '\0' && ! isspace(cmd_str[0])) {
			cmd_str++;
		}

		if (cmd_str[0] == '\0') break;

		cmd_str[0] = '\0';
		cmd_str++;
	}

	return args_found;
}

void usage(const char *name)
{
	fprintf(stderr,
		"usage: %s [options]\n"
		"\n"
		"Options:\n"
		" -d <path>	Path to serial device file\n"
		" -h		Print this help message\n"
		, name);
}


void print_pa(struct serco *sdev)
{
	int err;
	tPa_Setup pa_config;

	err = ser4010_get_pa(sdev, &pa_config);
	if (err != STATUS_OK) {
		fprintf(stderr, "Error getting PA config from device: err %d\n", err);
		return;
	}

	printf("fAlpha: %f\n", pa_config.fAlpha);
	printf("fBeta: %f\n", pa_config.fBeta);
	printf("bLevel: %u\n", pa_config.bLevel);
	printf("bMaxDrv: %u\n", pa_config.bMaxDrv);
	printf("wNominalCap: %u\n", pa_config.wNominalCap);
}
void print_ods(struct serco *sdev)
{
	int err;
	tOds_Setup ods_config;

	err = ser4010_get_ods(sdev, &ods_config);
	if (err != STATUS_OK) {
		fprintf(stderr, "Error getting ODS config from device: err %d\n", err);
		return;
	}

	printf("bModulationType: %s (%u)\n", modulation_type_to_str(ods_config.bModulationType), ods_config.bModulationType);
	printf("bClkDiv: %u\n", ods_config.bClkDiv);
	printf("bEdgeRate: %u\n", ods_config.bEdgeRate);
	printf("bGroupWidth: %u\n", ods_config.bGroupWidth);
	printf("wBitRate: %u\n", ods_config.wBitRate);
	printf("bLcWarmInt: %u\n", ods_config.bLcWarmInt);
	printf("bDivWarmInt: %u\n", ods_config.bDivWarmInt);
	printf("bPaWarmInt: %u\n", ods_config.bPaWarmInt);
}
void print_freq(struct serco *sdev)
{
	int err;
	float freq;

	err = ser4010_get_freq(sdev, &freq);
	if (err != STATUS_OK) {
		fprintf(stderr, "Error getting current frequency from device: err %d\n", err);
		return;
	}

	printf("fFreq: %f\n", freq);
}

void print_fdev(struct serco *sdev)
{
	int err;
	uint8_t fdev;

	err = ser4010_get_fdev(sdev, &fdev);
	if (err != STATUS_OK) {
		fprintf(stderr, "Error getting current frequency from device: err %d\n", err);
		return;
	}

	printf("fdev: %u\n", fdev);
}

void print_encoding(struct serco *sdev)
{
	int err;
	enum Ser4010Encoding encoding;

	err = ser4010_get_enc(sdev, &encoding);
	if (err != STATUS_OK) {
		fprintf(stderr, "Error getting current frequency from device: err %d\n", err);
		return;
	}

	printf("encoding: %s\n", encoding_to_str(encoding));
}

void cmd_pa(struct serco *sdev, size_t argc, char **argv)
{
	int err;
	char *endptr;
	unsigned long uint_arg;
	tPa_Setup rPaSetup;

	if (argc == 1) {
		print_pa(sdev);
		return;
	} else if (argc != 6) {
		printf("Command requires 0 or 5 arguments\n");
		return;
	}

	// parse Alpha
	rPaSetup.fAlpha = strtof(argv[1], &endptr);
	if (*endptr != '\0') {
		printf("fAlpha must be a floating point number\n");
		return;
	}

	// parse Beta
	rPaSetup.fBeta = strtof(argv[2], &endptr);
	if (*endptr != '\0') {
		printf("fBeta must be a floating point number\n");
		return;
	}

	// parse bLevel
	uint_arg = strtoul(argv[3], &endptr, 0);
	if (*endptr != '\0') {
		printf("bLevel must be a integer number\n");
		return;
	}
	if (uint_arg > 0x7f) {
		printf("bLevel out-of-range(0-128)\n");
		return;
	}
	rPaSetup.bLevel = uint_arg;

	// parse bMaxDrv
	if (strcmp(argv[4], "1") == 0 || strcasecmp(argv[4], "true") == 0) {
		rPaSetup.bMaxDrv = 1;
	} else if (strcmp(argv[4], "0") == 0 ||
			strcasecmp(argv[4], "false") == 0) {
		rPaSetup.bMaxDrv = 0;
	} else {
		printf("bMaxDrv must be either 1 or 0\n");
		return;
	}

	// parse wNominalCap
	uint_arg = strtoul(argv[5], &endptr, 0);
	if (*endptr != '\0') {
		printf("wNominalCap must be a integer number\n");
		return;
	}
	if (uint_arg > 0x1ff) {
		printf("wNominalCap out-of-range(0-512)\n");
		return;
	}
	rPaSetup.wNominalCap = uint_arg;

	err = ser4010_set_pa(sdev, &rPaSetup);
	if (err != STATUS_OK) {
		fprintf(stderr, "ser4010_set_pa() Failed: %d\n", err);
		return;
	}
}

void cmd_ods(struct serco *sdev, size_t argc, char **argv)
{
	int err;
	char *endptr;
	unsigned long uint_arg;
	tOds_Setup rOdsSetup;

	if (argc == 1) {
		print_ods(sdev);
		return;
	} else if (argc != 9) {
		printf("Command requires 0 or 8 arguments\n");
		return;
	}

	// parse bModulationType
	if (strcmp(argv[1], "0") == 0 || strcasecmp(argv[1], "ook") == 0) {
		rOdsSetup.bModulationType = ODS_MODULATION_TYPE_OOK;
	} else if (strcmp(argv[1], "1") == 0 ||
			strcasecmp(argv[1], "fsk") == 0) {
		rOdsSetup.bModulationType = ODS_MODULATION_TYPE_FSK;
	} else {
		printf("bModulationType must be either OOK or FSK\n");
		return;
	}

	// parse bClkDiv
	uint_arg = strtoul(argv[2], &endptr, 0);
	if (*endptr != '\0') {
		printf("bClkDiv must be a integer number\n");
		return;
	}
	if (uint_arg > 7) {
		printf("bClkDiv out-of-range(0-7)\n");
		return;
	}
	rOdsSetup.bClkDiv = uint_arg;

	// parse bEdgeRate
	uint_arg = strtoul(argv[3], &endptr, 0);
	if (*endptr != '\0') {
		printf("bEdgeRate must be a integer number\n");
		return;
	}
	if (uint_arg > 3) {
		printf("bEdgeRate out-of-range(0-3)\n");
		return;
	}
	rOdsSetup.bEdgeRate = uint_arg;

	// parse bGroupWidth
	uint_arg = strtoul(argv[4], &endptr, 0);
	if (*endptr != '\0') {
		printf("bGroupWidth must be a integer number\n");
		return;
	}
	if (uint_arg > 7) {
		printf("bGroupWidth out-of-range(0-7)\n");
		return;
	}
	rOdsSetup.bGroupWidth = uint_arg;

	// parse wBitRate
	uint_arg = strtoul(argv[5], &endptr, 0);
	if (*endptr != '\0') {
		printf("wBitRate must be a integer number\n");
		return;
	}
	if (uint_arg > 0x7fff) {
		printf("wBitRate out-of-range(0-32767)\n");
		return;
	}
	rOdsSetup.wBitRate = uint_arg;

	// parse bLcWarmInt
	uint_arg = strtoul(argv[6], &endptr, 0);
	if (*endptr != '\0') {
		printf("bLcWarmInt must be a integer number\n");
		return;
	}
	if (uint_arg > 15) {
		printf("bLcWarmInt out-of-range(0-15)\n");
		return;
	}
	rOdsSetup.bLcWarmInt = uint_arg;

	// parse bDivWarmInt
	uint_arg = strtoul(argv[7], &endptr, 0);
	if (*endptr != '\0') {
		printf("bDivWarmInt must be a integer number\n");
		return;
	}
	if (uint_arg > 15) {
		printf("bDivWarmInt out-of-range(0-15)\n");
		return;
	}
	rOdsSetup.bDivWarmInt = uint_arg;

	// parse bPaWarmInt
	uint_arg = strtoul(argv[8], &endptr, 0);
	if (*endptr != '\0') {
		printf("bPaWarmInt must be a integer number\n");
		return;
	}
	if (uint_arg > 15) {
		printf("bPaWarmInt out-of-range(0-15)\n");
		return;
	}
	rOdsSetup.bPaWarmInt = uint_arg;

	err = ser4010_set_ods(sdev, &rOdsSetup);
	if (err != STATUS_OK) {
		fprintf(stderr, "ser4010_set_ods() Failed: %d\n", err);
		return;
	}


}

void cmd_freq(struct serco *sdev, size_t argc, char **argv)
{
	int err;
	char *endptr;
	float freq;

	if (argc == 1) {
		print_freq(sdev);
		return;
	} else if (argc != 2) {
		printf("Command takes zero or one argument\n");
		return;
	}

	freq = strtof(argv[1], &endptr);
	if (*endptr != '\0') {
		printf("Argument 1 must be a floating point number\n");
		return;
	}
	if (freq < 27 * 1000 * 1000 || freq >= 960 * 1000 * 1000) {
		printf("frequency out-of-range(27e6 - 960e6)\n");
		return;
	}

	err = ser4010_set_freq(sdev, freq);
	if (err != STATUS_OK) {
		fprintf(stderr, "ser4010_set_freq() Failed: %d\n", err);
		return;
	}
}

void cmd_fdev(struct serco *sdev, size_t argc, char **argv)
{
	if (argc == 1) {
		print_fdev(sdev);
		return;
	} else if (argc != 2) {
		printf("Command takes zero or one argument\n");
		return;
	}

	printf("TODO:\n");
}

void cmd_encoding(struct serco *sdev, size_t argc, char **argv)
{
	if (argc == 1) {
		print_encoding(sdev);
		return;
	}

	printf("TODO:\n");
}

void cmd_frame(struct serco *sdev, size_t argc, char **argv)
{
	int err;
	uint8_t frame_buf[253];
	size_t byte_cnt;

	if (argc != 2) {
		printf("Command requires one argument\n");
		return;
	}

	byte_cnt = strlen(argv[1]) / 2;
	if (strlen(argv[1]) & 0x1) {
		printf("Encoded frame is not an even number of characters\n");
		return;
	}
	if (byte_cnt > sizeof(frame_buf)) {
		printf("Frame too long. Size is limited to %lu bytes\n", sizeof(frame_buf));
		return;
	}

	err = dehexify(argv[1], byte_cnt, frame_buf);
	if (err) {
		printf("Unable to parse encoded frame\n");
		return;
	}

	err = ser4010_load_frame(sdev, frame_buf, byte_cnt);
	if (err != STATUS_OK) {
		fprintf(stderr, "ser4010_load_frame() Failed: %d\n", err);
		return;
	}
}

void cmd_send(struct serco *sdev, size_t argc, char **argv)
{
	int err;
	unsigned int send_cnt = 1;

	if (argc == 2) {
		char *endptr;
		send_cnt = strtoul(argv[1], &endptr, 0);
		if (*endptr != '\0') {
			printf("Argument 1 must be a integer number\n");
			return;
		}
		if (send_cnt >= 0x100) {
			printf("Send count out-of-range(0-255)\n");
			return;
		}
	} else if (argc > 2) {
		printf("Command takes zero or one argument\n");
		return;
	}

	err = ser4010_send(sdev, send_cnt);
	if (err != STATUS_OK) {
		fprintf(stderr, "ser4010_send() Failed: %d\n", err);
		return;
	}
}

void cmd_config(struct serco *sdev, size_t argc, char **argv)
{
	int err;
	char *endptr;

	float freq_mhz;
	float fdev_khz;
	int modulation;
	enum Ser4010Encoding encoding;
	double data_rate_kbps;
	int bits_per_byte;

	if (argc != 7) {
		printf("Command takes 7 arguments\n");
		return;
	}

	// parse freq_mhz
	freq_mhz = strtof(argv[1], &endptr);
	if (*endptr != '\0') {
		printf("Argument 1 must be a floating point number\n");
		return;
	}
	if (freq_mhz < 27 || freq_mhz >= 960) {
		printf("frequency out-of-range(27 - 960)\n");
		return;
	}

	// parse fdev_khz
	fdev_khz = strtof(argv[2], &endptr);
	if (*endptr != '\0') {
		printf("Argument 2 must be a floating point number\n");
		return;
	}
	if (fdev_khz < 0) {
		printf("fdev must be positive\n");
		return;
	}

	// parse modulation
	if (strcmp(argv[3], "0") == 0 || strcasecmp(argv[3], "ook") == 0) {
		modulation = ODS_MODULATION_TYPE_OOK;
	} else if (strcmp(argv[3], "1") == 0 ||
			strcasecmp(argv[3], "fsk") == 0) {
		modulation = ODS_MODULATION_TYPE_FSK;
	} else {
		printf("Argument 3 must be either OOK or FSK\n");
		return;
	}

	// parse encoding
	if (strcasecmp(argv[4], "none") == 0) {
		encoding = bEnc_NoneNrz_c;
	} else if (strcasecmp(argv[4], "manchester") == 0) {
		encoding = bEnc_Manchester_c;
	} else if (strcasecmp(argv[4], "4b5b") == 0) {
		encoding = bEnc_4b5b_c;
	} else {
		printf("Argument 4 must be one of: none, manchester, 4b5b\n");
		return;
	}

	// parse data_rate_kbps
	data_rate_kbps = strtod(argv[5], &endptr);
	if (*endptr != '\0') {
		printf("Argument 5 must be a floating point number\n");
		return;
	}
	if (data_rate_kbps < (24000.0/(8*0x7fff)) ||
			data_rate_kbps > 1000) {
		// Absolute max would be: (24000.0/(1*1)) / 2
		// Or 1/2 of that when using manchester encoding.
		// But limit some more to be sane
		printf("data rate out-of-range(%.3f - 1000)\n",
				(24000.0/(8*0x7fff)));
		return;
	}

	// parse bits_per_byte
	bits_per_byte = strtoul(argv[6], &endptr, 0);
	if (*endptr != '\0') {
		printf("Argument 6 must be a integer number\n");
		return;
	}
	if (bits_per_byte < 1 || bits_per_byte > 8) {
		printf("bits_per_byte out-of-range(1-8)\n");
		return;
	}

	err = ser4010_config(sdev, freq_mhz, fdev_khz, modulation, encoding,
			data_rate_kbps, bits_per_byte);
	if (err != STATUS_OK) {
		fprintf(stderr, "ser4010_send() Failed: %d\n", err);
		return;
	}

	// TODO: maybe read back what is actually configured? This doesn't have to match the requested parameters...
}

void cmd_ping(struct serco *sdev, size_t argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);
	int err;

	err = serco_send_command(sdev, CMD_NOP, NULL, 0, NULL, 0);
	if (err != STATUS_OK) {
		if (err > 0) {
			printf("ser4010 returned error status: 0x%02x\n", err);
		} else {
			printf("Communication Failed\n");
		}
	} else {
		printf("Communication OK\n");
	}
}

void cmd_info(struct serco *sdev, size_t argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);
	int err;
	uint16_t dev_type;
	uint16_t dev_rev;

	err = ser4010_get_dev_type(sdev, &dev_type);
	if (err) {
		fprintf(stderr, "Failed to obtain device type: err %d\n", err);
		return;
	}

	err = ser4010_get_dev_rev(sdev, &dev_rev);
	if (err) {
		fprintf(stderr, "Failed to obtain device revision: err %d\n", err);
		return;
	}

	printf("Device type: 0x%04x\n", dev_type);
	if (dev_type != SER4010_DEV_TYPE) {
		printf("ERROR: Device is not a Ser4010 device!\n");
	}

	printf("Device revision: 0x%04x\n", dev_rev);
	if (dev_rev != SER4010_DEV_REV) {
		printf("Warning: Revision mismatch with compiled tool\n");
	}
}

void cmd_help(struct serco *sdev, size_t argc, char **argv)
{
	UNUSED(sdev);

	if (argc > 2) {
		printf("Too many arguments for help command\n");
		return;
	} else if (argc == 1) {
	printf(
"Commands:\n"
" info\n"
"   Print ser401 device info\n"
"\n"
" config <freq_MHz> <fdev_kHz> <OOK|FSK> <encoding> <rate_kbps> <bits_per_byte>\n"
"   High level device configuration interface\n"
"\n"
" pa <fAlpha> <fBeta> <bLevel> <bMaxDrv> <wNominalCap>\n"
"   Configure the Power Amplifier.\n"
"   If no parameters supplied current settings are printed.\n"
"\n"
" ods <...>\n"
"   Configure Output Data Serializer.\n"
"   If no parameters supplied current settings are printed.\n"
"\n"
" freq <fFreq>\n"
"   Set transmission frequency.\n"
"   If no parameters supplied current settings are printed.\n"
"\n"
" fdev <fFdev>\n"
"   Set FSK frequency deviation. Note that fFdev is a unitless number obtained\n"
"   using the Si4010 calculation spreadsheet.\n"
"   If no parameters supplied current settings are printed.\n"
"\n"
" encoding <encoding>\n"
"   Set which data encoding scheme to apply to the frame data before sending.\n"
"   Valid encodings: none, manchester, 4b5b.\n"
"   If no parameters supplied current settings are printed.\n"
"\n"
" frame <frame_data>\n"
"   Load frame data for transmission. Frame data is a hexadecimal encoded\n"
"   sequence of bytes(ie. 0011eeff). Bytes are transmitted starting at the\n"
"   LSB.\n"
"\n"
" send [N]\n"
"   Transmit one, or if provided N, frame(s).\n"
"\n"
" ping\n"
"   Test if device is responding.\n"
"\n"
" help <command>\n"
"   Print this help message.\n"
		);
	} else if (strcasecmp(argv[1], "info") == 0) {
	} else if (strcasecmp(argv[1], "config") == 0) {
	} else if (strcasecmp(argv[1], "pa") == 0) {
	} else if (strcasecmp(argv[1], "ods") == 0) {
	} else if (strcasecmp(argv[1], "freq") == 0) {
	} else if (strcasecmp(argv[1], "fdev") == 0) {
	} else if (strcasecmp(argv[1], "encoding") == 0) {
	} else if (strcasecmp(argv[1], "frame") == 0) {
	} else if (strcasecmp(argv[1], "send") == 0) {
	} else if (strcasecmp(argv[1], "ping") == 0) {
		printf("Sends a No-operation command to device and checks "
				"response.\n");
	} else {
		printf("No help available for that command.\n");
	}
}

int check_device(struct serco *sdev)
{
	int err;
	uint16_t dev_type;
	uint16_t dev_rev;

	err = serco_send_command(sdev, CMD_NOP, NULL, 0, NULL, 0);
	if (err != STATUS_OK) {
		printf("Unable to communicate with device");
		if (err > 0) {
			printf(": ser4010 returned error status: 0x%02x\n", err);
		} else {
			putchar('\n');
		}
		return 1;
	}

	err = ser4010_get_dev_type(sdev, &dev_type);
	if (err) {
		fprintf(stderr, "Failed to obtain device type: err %d\n", err);
		return 1;
	}
	if (dev_type != SER4010_DEV_TYPE) {
		fprintf(stderr, "Incorrect device type: %d\n", dev_type);
		return 1;
	}

	err = ser4010_get_dev_rev(sdev, &dev_rev);
	if (err) {
		fprintf(stderr, "Failed to obtain device revision: err %d\n", err);
		return 1;
	}
	if (dev_rev != SER4010_DEV_REV) {
		fprintf(stderr, "Incorrect device revision: %d\n", dev_rev);
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int opt;

	char *dev_path;

	struct serco sdev;

	char prompt[256] = "ser4010> ";
	char *line_buf = NULL;
	char *line_argv[255];
	size_t line_argc;

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

	// open/init SER4010
	if (serco_open(&sdev, dev_path) != 0) {
		exit(EXIT_FAILURE);
	}

	if (check_device(&sdev) != 0) {
		exit(EXIT_FAILURE);
	}

	printf("Connected to device %s\n", dev_path);

	while (true) {
		line_buf = readline(prompt);

		char *line = line_buf;

		if (line == NULL) {
			putchar('\n');
				// EOF means no New line is entered after
				// prompt. So add one to make sure any
				// subsequent output(eg. console prompt) start
				// at a new line
			break;
		}

		add_history(line);

		line_argc = str_to_args(line, line_argv, ARRAY_SIZE(line_argv));

		if (line_argc < 1) {
			// Ignore empty line
		} else if (strcasecmp(line_argv[0], "quit") == 0) {
			break;
		} else if (strcasecmp(line_argv[0], "help") == 0) {
			cmd_help(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "info") == 0) {
			cmd_info(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "config") == 0) {
			cmd_config(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "ping") == 0) {
			cmd_ping(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "pa") == 0) {
			cmd_pa(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "ods") == 0) {
			cmd_ods(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "freq") == 0) {
			cmd_freq(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "fdev") == 0) {
			cmd_fdev(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "encoding") == 0) {
			cmd_encoding(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "frame") == 0) {
			cmd_frame(&sdev, line_argc, line_argv);
		} else if (strcasecmp(line_argv[0], "send") == 0) {
			cmd_send(&sdev, line_argc, line_argv);
		} else {
			printf("Unknown command\n");
		}

		free(line_buf);
		line_buf = NULL;
	}

	serco_close(&sdev);
	free(line_buf);

	return 0;
}
