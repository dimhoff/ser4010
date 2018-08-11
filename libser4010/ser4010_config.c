/**
 * ser4010_config.c - Implement the ser4010_config() high level SER4010 RF
 *			configuration API
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
#include "ser4010.h"

#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "si4010_tables.h"

static float lookup_float_by_float(const float table[][2], float key)
{
	const float (*row)[2] = NULL;
	float last_val = 0;

	for (row=&(table[0]); !isnan((*row)[0]); row++) {
		if ((*row)[0] > key) {
			return last_val;
		}
		last_val = (*row)[1];
	}

	return last_val;
}

static void calc_best_ramp_param(float ramp_time, uint8_t *clk_div,
				uint8_t *edge_rate)
{
	uint8_t i;
	uint8_t j;
	float best_ramp_diff = INFINITY;
	for (i = 1; i < 5; i++) {
		for (j = 1; j < 9; j++) {
			float diff = fabs(ramp_time - (i * j * 8.0 / 24));
			if (diff < best_ramp_diff) {
				best_ramp_diff = diff;
				*clk_div = j - 1;
				*edge_rate = i - 1;
			}
		}
	}
}


static int config_ods(tOds_Setup *ods_config,
			int modulation, enum Ser4010Encoding encoding,
			double data_rate_kbps, int bits_per_byte)
{
	double sym_rate_ksym_sec;

	// Determine symbol rate
	switch (encoding) {
	case bEnc_NoneNrz_c:
		sym_rate_ksym_sec = data_rate_kbps;
		if (bits_per_byte < 1 || bits_per_byte > 8) {
			return EINVAL;
		}
		break;
	case bEnc_Manchester_c:
		sym_rate_ksym_sec = 2 * data_rate_kbps;
		if (bits_per_byte != 8) {
			return EINVAL;
		}
		break;
	case bEnc_4b5b_c:
		sym_rate_ksym_sec = 1.25 * data_rate_kbps;
		if (bits_per_byte != 5) {
			return EINVAL;
		}
		break;
	//case "Other":
	//	sym_rate_ksym_sec = symbols_per_bit * data_rate_kbps;
	//	break;
	default:
		return EINVAL;
	}

	// Determine ramp time for calculating clk_div and edge_rate
	float want_ramp_time;
	if (modulation == ODS_MODULATION_TYPE_OOK) {
		if (encoding == bEnc_Manchester_c) {
			want_ramp_time = lookup_float_by_float(
						ramp_data_manchester,
						sym_rate_ksym_sec/2);
		} else {
			want_ramp_time = lookup_float_by_float(
						ramp_data_nrz,
						sym_rate_ksym_sec);
		}
	} else if (modulation == ODS_MODULATION_TYPE_FSK) {
		// FSK
		if (sym_rate_ksym_sec < 1) {
			want_ramp_time = 8;
		} else {
			want_ramp_time = 2;
		}
	} else {
		return EINVAL;
	}

	// TODO: assert on incorrect bit rate

	// Setup the ODS 
	ods_config->bModulationType = modulation;
	calc_best_ramp_param(want_ramp_time,
				&ods_config->bClkDiv, &ods_config->bEdgeRate);
	ods_config->bGroupWidth    = bits_per_byte - 1;
	ods_config->wBitRate       = 24000.0 / (sym_rate_ksym_sec *
						(ods_config->bClkDiv+1));
	if (10 * (24000 / (ods_config->wBitRate * (ods_config->bClkDiv+1)))
		>= 76)
	{
		// Note: this is NOT the same as (10 * sym_rate_ksym_sec),
		// because the conversion to uint16_t does a implicit floor()
		ods_config->bLcWarmInt = 0;
	} else {
		ods_config->bLcWarmInt = ceil(24.0*125 /
						((ods_config->bClkDiv+1)*64));
	}
	ods_config->bDivWarmInt = ceil(24.0*5 / ((ods_config->bClkDiv+1)*4));
	ods_config->bPaWarmInt  = ceil(24.0 / (ods_config->bClkDiv+1));

	return 0;
}

static uint8_t lookup_fdev(float freq_mhz, float fdev_khz)
{
	float wanted_shift;
	size_t min_div_idx;
	float min_div;
	size_t i;

	wanted_shift = (fdev_khz * 2000.0 / freq_mhz);

	min_div_idx = 0;
	min_div = abs(wanted_shift - ppm_shift[0]);
	for (i=1; i < sizeof(ppm_shift)/sizeof(ppm_shift[0]); i++) {
		double div = abs(wanted_shift - ppm_shift[i]);
		if (div < min_div) {
			min_div_idx = i;
			min_div = div;
		}
	}

	if (min_div_idx > 104) {
		min_div_idx = 104;
	}
	
	return min_div_idx;
}

int ser4010_config(struct serco *sdev,
			float freq_mhz, float fdev_khz,
			int modulation, enum Ser4010Encoding encoding,
			double data_rate_ksym_sec, int bits_per_byte)
{
	int err;
	tOds_Setup ods_config;

	err = config_ods(&ods_config, modulation, encoding, data_rate_ksym_sec,
			bits_per_byte);
	if (err != 0) {
		return err;
	}

	err = ser4010_set_ods(sdev, &ods_config);
	if (err != 0) {
		return err;
	}

	err = ser4010_set_freq(sdev, freq_mhz * 1e6);
	if (err != 0) {
		return err;
	}

	err = ser4010_set_enc(sdev, encoding);
	if (err != 0) {
		return err;
	}

	if (modulation == ODS_MODULATION_TYPE_FSK) {
		uint8_t fdev;

		fdev = lookup_fdev(freq_mhz, fdev_khz);

		err = ser4010_set_fdev(sdev, fdev);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}
