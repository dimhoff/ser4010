/**
 * ser4010_si443x.c - Send Silicon Labs Si443x compatible frames
 *
 * ----------------------------------------------------------------------------
 * WARNING: This code might not work. My current Si4432 module is unable to
 * receive the frames unless using OOK at a bitrate of 9k6 at a distance of
 * < 30 cm. Analyzing the signals with SDR shows that the bit timing seems ok.
 * So I hope this is just caused by a broken Si4432. But till I prove this it
 * can as well be a fundamental incompatibility between the Si4010 and Si443x.
 * Just be aware of this...
 * ----------------------------------------------------------------------------
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
#include <assert.h>

#include "dehexify.h"
#include "serco.h"
#include "ser4010.h"
#include "crc_16.h"
#include "pn9.h"

#define DEVICE "/dev/ttyUSB0"

/**
 * Reverse bit order in a byte
 */
static inline uint8_t reverse_byte(uint8_t b)
{
	b = ((b & 0xaa) >> 1) | ((b & 0x55) << 1);
	b = ((b & 0xcc) >> 2) | ((b & 0x33) << 2);
	b = ((b & 0xf0) >> 4) | ((b & 0x0f) << 4);

	return b;
}

// Polynomials according to AN625
// IEC-16:       X16+X14+X12+X11+X9+X8+X7+X4+X+1
// Baicheva-16:  X16+X15+X12+X7+X6+X4+X3+1
// CRC-16 (IBM): X16+X15+X2+1
// CCIT-16:      X16+X12+X5+1
#define POLY_IEC_16	(0x5B93)
#define POLY_BAICHEVA	(0x90D9)
#define POLY_CRC_16	(0x8005)
#define POLY_CCITT_16	(0x1021)

enum CrcType {
	CCITT,
	CRC_16,
	IEC_16,
	Baicheva
};
// Silicon Labs doc's say Biacheva, but I assume they meant Tsonka Baicheva.
#define Biacheva Baicheva

struct si443x_cfg {
	uint16_t preamble_len;		// reg 0x33[0] || 0x34
	bool preamble_polarity;		// reg 0x70[3]
	uint8_t sync_len;		// reg 0x33[2:1]
	uint8_t sync_word[4];		// reg 0x36-0x39
	uint8_t hdr_len;		// reg 0x33[6:4]
	uint8_t hdr[4];
	bool fixed_pkt_len;		// reg 0x33[3]
	bool crc_enabled;		// reg 0x30[2]
	bool crc_data_only;		// reg 0x30[5]
	enum CrcType crc_type;		// reg 0x30[1:0]
	bool lsb_first;			// reg 0x30[6]
	bool encoding_enabled;		// reg 0x70[1]
	bool manchester_inverse;	// reg 0x70[2]
	bool whitening_enabled;		// reg 0x70[0]
};

#define EZRADIOPRO_CFG_INITIALIZER \
	{ \
		8, \
		true, \
		2, \
		{ 0x2d, 0xd4, 0, 0 }, \
		2, \
		{ 0 }, \
		false, \
		true, \
		false, \
		CRC_16, \
		false, \
		false, \
		true, \
		false \
	}
		
	
int ser4010_si443x_send(struct serco *sdev,
			uint8_t *payload, size_t payload_len,
			const struct si443x_cfg *cfg)
{
	int ret;

  	uint8_t buf[256];
	uint8_t *bp = buf;
	uint8_t *pkt_start = NULL;
	uint8_t *data_start = NULL;

	uint8_t preamble_byte;
	unsigned int preamble_byte_cnt;

	uint8_t manchester_invert;

	assert(payload_len < 250);

	// Preamble
	// Only support a preamble length of a multiple of 8-bit, round up if not.
	preamble_byte_cnt = (cfg->preamble_len + 1) / 2;
	if (cfg->encoding_enabled) {
		if (cfg->preamble_polarity) {
			preamble_byte = 0xff;
		} else {
			preamble_byte = 0x00;
		}
	} else {
		// NOTE: preamble is always manchester encoded.
		// TODO: does this mean a preamble length of 1 nibble is actually manchester encoded 1 byte???
		//       if so, then preamble_byte_cnt == cfg->preamble_len.
		// TODO: lsb_first doesn't seem to apply to preamble, but doesn't seem to matter
		if (cfg->preamble_polarity) {
			preamble_byte = 0x55;
		} else {
			preamble_byte = 0xaa;
		}
	}
	memset(bp, preamble_byte, preamble_byte_cnt);
	bp += preamble_byte_cnt;

	// Sync Word
	assert(cfg->sync_len <= 4 && cfg->sync_len > 0);
	memcpy(bp, cfg->sync_word, cfg->sync_len);
	bp += cfg->sync_len;
	pkt_start = bp;

	// Header
	assert(cfg->hdr_len <= 4 && cfg->hdr_len >= 0);
	if (cfg->hdr_len) {
		memcpy(bp, cfg->hdr, cfg->hdr_len);
		bp += cfg->hdr_len;
	}

	// Packet Len.
	if (! cfg->fixed_pkt_len) {
		*bp = payload_len;
		bp += 1;
	}

	// Data
	data_start = bp;
	memcpy(bp, payload, payload_len);
	bp += payload_len;

	// CRC
	if (cfg->crc_enabled) {
		uint16_t crc = 0;
		uint16_t polynomial = 0;
		uint8_t *p;

		switch (cfg->crc_type) {
		case CCITT:
			polynomial = POLY_CCITT_16;
			break;
		case CRC_16:
			polynomial = POLY_CRC_16;
			break;
		case IEC_16:
			polynomial = POLY_IEC_16;
			break;
		case Baicheva:
			polynomial = POLY_BAICHEVA;
			break;
		default:
			assert(false);
		}

		if (cfg->crc_data_only) {
			p = data_start;
		} else {
			p = pkt_start;
		}

		while (p < bp) {
			uint8_t b = *p;
			if (cfg->lsb_first) {
				b = reverse_byte(b);
			}
			crc = crc_16(crc, b, polynomial);
			p++;
		}
		if (cfg->lsb_first) {
			crc = (reverse_byte(crc >> 8) << 8) | reverse_byte(crc & 0xff);
		}

		bp[0] = crc >> 8;
		bp[1] = crc & 0xff;
		bp += 2;
	}

	// Data Whitening
	if (cfg->whitening_enabled) {
		uint16_t pn9 = PN9_INITIALIZER;
		uint8_t *p;

		for (p = pkt_start; p < bp; p++) {
			pn9 = pn9_next_byte(pn9);
			*p ^= reverse_byte(pn9>>1);
		}
	}

	// Manchester Encoding
	manchester_invert = 0x00;
	if (cfg->encoding_enabled && cfg->manchester_inverse) {
		manchester_invert = 0xff;
	}

	// LSB first
	if (! cfg->lsb_first || manchester_invert) {
		uint8_t *p;
		for (p = buf; p < bp; p++) {
			if (! cfg->lsb_first) { // Si4010 does LSB first by default.
				*p = reverse_byte(*p);
			}
			*p ^= manchester_invert;
		}
	}

	ret = ser4010_load_frame(sdev, buf, (bp - buf));
	if (ret != STATUS_OK) {
		return ret;
	}

	ret = ser4010_send(sdev, 1);
	if (ret != STATUS_OK) {
		return ret;
	}

	return STATUS_OK;
}

void usage(const char *name)
{
	fprintf(stderr,
		"Send Si443x compatible frames\n"
"----------------------------------------------------------------------------\n"
"WARNING: This code might not work. My current Si4432 module is unable to\n"
"receive the frames unless using OOK at a bitrate of 9k6 at a distance of\n"
"< 30 cm. Analyzing the signals with SDR shows that the bit timing seems ok.\n"
"So I hope this is just caused by a broken Si4432. But till I prove this it\n"
"can as well be a fundamental incompatibility between the Si4010 and Si443x.\n"
"Just be aware of this...\n"
"----------------------------------------------------------------------------\n"
		"\n"
		"usage: %s [options] <data>\n"
		"\n"
		"Options:\n"
		"  -f, --frequency=FREQ      Carrier frequency in MHz (default: 433.9 MHz)\n"
		"  -m, --modulation=MOD      Modulation scheme: OOK or FSK (default: OOK)\n"
		"  --fsk-deviation=FDEV      FSK frequency deviation in kHz (default: 50 kHz)\n"
		"                            Value should be in the range 1-130. Note that the\n"
		"                            actual max. deviation is clipped at about 135 ppm\n"
		"                            of the carrier frequency.\n"
		"  -r, --bit-rate=RATE       Bit rate in kbit/s (default: 9.6)\n"
		"  --preamble-length=LEN     Preamble length in byte (default: 4)\n"
		"  --preamble-invert         Invert preamble\n"
		"  --sync-word=SYNC_WORD     Sync word as 1-4 byte hex. string (default: 2dd4)\n"
		"  --header=HEADER           Header bytes as 0-4 byte hex. string (default: 1122)\n"
		"  --fixed-length            Don't include packet length\n"
		"  -c, --crc=TYPE            Use CRC type (default: crc-16)\n"
		"                            types: none, ccitt, crc-16, iec-16, baicheva\n"
		"  --crc-data-only           Calculate CRC on data only\n"
		"  --lsb-first               Send bytes LSB first\n"
		"  -e, --encoding-enabled    Enable Manchester encoding\n"
		"  --manchester-no-invert    Don't invert Manchester encoding\n"
		"  -w, --whitening-enabled   Enable data whitening\n"
		"  -x, --hex                 Input data is encoded as hexadecimal string\n"
		"  -h, --help                Print this help message\n"
		"\n"
		"Arguments:\n"
		"  data: The data to send. When the -x option is given the data is interpreted\n"
		"        as hexadecimal string\n"
		, name);
}

int main(int argc, char *argv[])
{
	const char *dev_path = DEVICE;
	struct serco sdev;

	bool hex_input = false;
	struct si443x_cfg cfg = EZRADIOPRO_CFG_INITIALIZER;
	float freq = 433.9;
	float fdev = 50;
	int modulation = ODS_MODULATION_TYPE_OOK;
	float bit_rate = 9.6;
	uint8_t data[64];
	int data_len;

	int ret;

	cfg.hdr[0] = 0x11;
	cfg.hdr[1] = 0x22;
	cfg.hdr[2] = 0x33;
	cfg.hdr[3] = 0x44;

	// Option parsing
	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "frequency",         required_argument,  0, 'f' },
			{ "modulation",        required_argument,  0, 'm' },
			{ "fsk-deviation",     required_argument,  0,  0  },
			{ "bit-rate",          required_argument,  0, 'r' },
			{ "preamble-length",   required_argument,  0,  0  },
			{ "preamble-invert",   no_argument,        0,  0  },
			{ "sync-word",         required_argument,  0,  0  },
			{ "header",            required_argument,  0,  0  },
			{ "fixed-length",      no_argument,        0,  0  },
			{ "crc",               required_argument,  0, 'c' },
			{ "crc-data-only",     no_argument,        0,  0  },
			{ "lsb-first",         no_argument,        0,  0  },
			{ "encoding-enabled",  no_argument,        0, 'e' },
			{ "manchester-no-invert", no_argument,     0,  0  },
			{ "whitening-enabled", no_argument,        0, 'w' },
			{ "hex",               no_argument,        0, 'x' },
			{ "help",              no_argument,        0, 'h' },
			{ 0, 0, 0, 0 }
		};
		int c;
		char *endp;

		c = getopt_long(argc, argv, "f:m:r:c:ewxh",
				long_options, &option_index);
		if (c == -1)
			break;

		if (c == 0) {
			const char *optname = long_options[option_index].name;

			if (strcmp(optname, "fsk-deviation") == 0) {
				fdev = strtof(optarg, &endp);
				if (endp == NULL || *endp != '\0') {
					fprintf(stderr, "Frequency deviation"
						"not a valid number\n");
					exit(EXIT_FAILURE);
				}
				if (fdev > 1 || freq > 130) {
					fprintf(stderr,
						"Frequency deviation out of "
						"range (1 < freq < 130)\n");
					exit(EXIT_FAILURE);
				}

			} else if (strcmp(optname, "preamble-length") == 0) {
				cfg.preamble_len = strtol(optarg, &endp, 0);
				if (endp == NULL || *endp != '\0') {
					fprintf(stderr, "Preamble lengthi is "
						"not a valid number\n");
					exit(EXIT_FAILURE);
				}

				if (cfg.preamble_len < 1 ||
						cfg.preamble_len > 255) {
					fprintf(stderr, "Preamble length must "
							"be in range 1-255\n");
					exit(EXIT_FAILURE);
				}
			} else if (strcmp(optname, "preamble-invert") == 0) {
				cfg.preamble_polarity = false;
			} else if (strcmp(optname, "sync-word") == 0) {
				if (strlen(optarg) & 1) {
					fprintf(stderr, "Hexified sync word"
						"string length must be a "
						"multiple of 2\n");
					exit(EXIT_FAILURE);
				}
				cfg.sync_len = strlen(optarg)/2;
				if (cfg.sync_len < 1 || cfg.sync_len > 4) {
					fprintf(stderr, "Sync word can only be "
						"1 to 4 bytes long\n");
					exit(EXIT_FAILURE);
				}
				if (dehexify(optarg, cfg.sync_len,
						cfg.sync_word) != 0) {
					fprintf(stderr, "Unable to dehexify "
						"sync word bytes\n");
					exit(EXIT_FAILURE);
				}
			} else if (strcmp(optname, "header") == 0) {
				if (strlen(optarg) & 1) {
					fprintf(stderr, "Hexified header"
						"string length must be a "
						"multiple of 2\n");
					exit(EXIT_FAILURE);
				}
				cfg.hdr_len = strlen(optarg)/2;
				if (cfg.hdr_len > 4) {
					fprintf(stderr, "Header can only be "
						"0 to 4 bytes long\n");
					exit(EXIT_FAILURE);
				}
				if (dehexify(optarg, cfg.hdr_len,
						cfg.hdr) != 0) {
					fprintf(stderr, "Unable to dehexify "
						"header bytes\n");
					exit(EXIT_FAILURE);
				}
			} else if (strcmp(optname, "fixed-length") == 0) {
				cfg.fixed_pkt_len = true;
			} else if (strcmp(optname, "crc-data-only") == 0) {
				cfg.crc_data_only = true;
			} else if (strcmp(optname, "lsb-first") == 0) {
				cfg.lsb_first = true;
			} else if (strcmp(optname, "manchester-no-invert")
					== 0) {
				cfg.manchester_inverse = true;
			}
		} else {
			switch (c) {
			case 'f':
				freq = strtof(optarg, &endp);
				if (endp == NULL || *endp != '\0') {
					fprintf(stderr, "Carrier frequency "
						"not a valid number\n");
					exit(EXIT_FAILURE);
				}
				if (freq > 960 || freq < 240) {
					fprintf(stderr,
						"Carrier frequency out of "
						"range (240 < freq < 960)\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'm':
				if (strcasecmp(optarg, "OOK") == 0) {
					modulation = ODS_MODULATION_TYPE_OOK;
				} else if (strcasecmp(optarg, "FSK") == 0) {
					modulation = ODS_MODULATION_TYPE_FSK;
				} else {
					fprintf(stderr, "Invalid modulation "
						"type\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'r':
				bit_rate = strtof(optarg, &endp);
				if (endp == NULL || *endp != '\0') {
					fprintf(stderr, "Bit rate "
						"not a valid number\n");
					exit(EXIT_FAILURE);
				}
				if (bit_rate < 0.123 || bit_rate > 50) {
					fprintf(stderr,
						"Bit rate out of "
						"range (0.123 < freq < 50)\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'c':
				if (strcasecmp(optarg, "none") == 0) {
					cfg.crc_enabled = false;
				} else if (strcasecmp(optarg, "ccitt") == 0) {
					cfg.crc_enabled = true;
					cfg.crc_type = CCITT;
				} else if (strcasecmp(optarg, "crc-16") == 0) {
					cfg.crc_enabled = true;
					cfg.crc_type = CRC_16;
				} else if (strcasecmp(optarg, "iec-16") == 0) {
					cfg.crc_enabled = true;
					cfg.crc_type = IEC_16;
				} else if (strcasecmp(optarg, "baicheva") == 0) {
					cfg.crc_enabled = true;
					cfg.crc_type = Baicheva;
				} else if (strcasecmp(optarg, "biacheva") == 0) {
					cfg.crc_enabled = true;
					cfg.crc_type = Baicheva;
				} else {
					fprintf(stderr, "Invalid CRC type\n");
					exit(EXIT_FAILURE);
				}

				printf("option c with value '%s'\n", optarg);
				break;
			case 'e':
				cfg.encoding_enabled = true;
				break;
			case 'w':
				cfg.whitening_enabled = true;
				break;
			case 'x':
				hex_input = true;
				break;
			case 'h':
				usage(argv[0]);
				exit(EXIT_SUCCESS);
				break;
			case '?':
				fprintf(stderr, "Unknown option: %c\n", optopt);
				usage(argv[0]);
				exit(EXIT_FAILURE);
				break;
			default:
				fprintf(stderr, "illegal option %c\n", c);
				usage(argv[0]);
				exit(EXIT_FAILURE);
				break;
			}
		}
	}

	// Argument parsing
	if (argc - optind > 1) {
		fprintf(stderr, "Too many arguments\n");
		exit(EXIT_FAILURE);
	}

	if (argc - optind == 0) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	char *data_arg;
	if (strcmp(argv[optind], "-") == 0) {
		// TODO: read from STDIN
		fprintf(stderr, "TODO: implement\n");
		exit(EXIT_FAILURE);
	} else {
		data_arg = argv[optind];
	}

	if (hex_input) {
		if (strlen(data_arg) & 1) {
			fprintf(stderr, "Data must consist of a even amount of bytes\n");
			exit(EXIT_FAILURE);
		}
		data_len = strlen(data_arg) / 2;
		if (data_len > sizeof(data)) {
			fprintf(stderr, "Data can not be longer than %zu bytes\n", sizeof(data));
			exit(EXIT_FAILURE);
		}
		if (dehexify(data_arg, data_len, data) != 0) {
			fprintf(stderr, "Unable to dehexify data\n");
			exit(EXIT_FAILURE);
		}
	} else {
		data_len = strlen(data_arg);
		if (data_len > sizeof(data)) {
			fprintf(stderr, "Data can not be longer than %zu bytes\n", sizeof(data));
			exit(EXIT_FAILURE);
		}
		memcpy(data, data_arg, data_len);
	}

	// open/init SER4010
	if (serco_open(&sdev, dev_path) != 0) {
		exit(EXIT_FAILURE);
	}

	enum Ser4010Encoding enc = bEnc_NoneNrz_c;
	if (cfg.encoding_enabled) {
		enc = bEnc_Manchester_c;
	}

	//TODO: move these param to config struct?
	ret = ser4010_config(&sdev, freq, fdev, modulation, enc, bit_rate, 8);
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
	ret = ser4010_si443x_send(&sdev, data, data_len, &cfg);

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
