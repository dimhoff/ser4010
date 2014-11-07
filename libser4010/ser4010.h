/**
 * ser4010.h - SER4010 API interface
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
#ifndef __SER4010_H__
#define __SER4010_H__

#include <stdint.h>

#include "serco.h"

//NOTE: the float type MUST be a IEEE-754 'single' floating point number

/**
 * Output Data Serializer configuration structure
 */
#pragma pack(1)
typedef struct {
	uint8_t  bModulationType; /**< Modulation type: 0=OOK, 1=FSK */
#define ODS_MODULATION_TYPE_OOK 0
#define ODS_MODULATION_TYPE_FSK 1
	uint8_t  bClkDiv;
	uint8_t  bEdgeRate;       /* Only 2 LSb used */
	uint8_t  bGroupWidth;
		/**< Only 3 LSB used, When sending data only the first
		 * (bGroupWidth + 1) bits in the output data will be used. This
		 * is useful when your encoding maps on bit to a odd amount of
		 * symbols. eg. 1-bit is encoded into 3 symbols, then use
		 * bGroupwidth = 5 and pack 2 input bits per output byte. This
		 * prevents having to break one encoded bit across an output
		 * byte boundery.
		 */
	uint16_t wBitRate;        /* Only bits 14 through 0 used */
	// Bit width in seconds = (ods_datarate*(ods_ck_div+1))/24MHz
	uint8_t  bLcWarmInt;
	uint8_t  bDivWarmInt;
	uint8_t  bPaWarmInt;
} tOds_Setup;
#pragma pack()

/**
 * Power Amplifier configuration structure
 */
#pragma pack(1)
typedef struct {
	float    fAlpha;
		/**< Use si4010_calc_regs_110107.xls to calculate. */
	float    fBeta;
		/**< Use si4010_calc_regs_110107.xls to calculate. */
	uint8_t  bLevel;
		/**< AN370 Rev. 1.0 page 77:
		 * 7-bit PA transmit power level.
		 */
	uint8_t  bMaxDrv;
		/**< AN370 Rev. 1.0 page 77:
		 * Boost bias current to output DAC. Allows for maximum 10.5mA
		 * drive. Only LSb bit (bit 0) is used.
		 */
	uint16_t wNominalCap;
		/**< SI4010-C2 Rev. 1.0 page 38:
		 * 9-bit linear control value of the output capacitance of the
		 * PA. Accessed as 2 bytes (word) in big-endian fashion. Upper
		 * bits [15:9] are read as 0. Range: 2.4–12.5 pF (not exact
		 * values). The resonance frequency and impedance matching
		 * between the PA output and the connected antenna can be tuned
		 * by changing this value.
		 */
} tPa_Setup;
#pragma pack()

/**
 * Data encoding types for vStl_EncodeSetup()
 */
enum Ser4010Encoding {
	bEnc_NoneNrz_c    = 0,   /**< No encoding */
	bEnc_Manchester_c = 1,   /**< Manchester encoding */
	bEnc_4b5b_c       = 2,   /**< 4b-5b encoding */
};


/**
 * Set Output Data Serializer configuration
 *
 * @param sdev		Serial Communication handle
 * @param ods_config	New configuration
 */
int ser4010_set_ods(struct serco *sdev, const tOds_Setup *ods_config);

/**
 * Get Output Data Serializer configuration
 *
 * @param sdev		Serial Communication handle
 * @param ods_config	Structure to return configuration in
 */
int ser4010_get_ods(struct serco *sdev, tOds_Setup *ods_config);

/**
 * Set Power Amplifier configuration
 *
 * @param sdev		Serial Communication handle
 * @param ods_config	New configuration
 */
int ser4010_set_pa(struct serco *sdev, const tPa_Setup *pa_config);

/**
 * Get Power Amplifier configuration
 *
 * @param sdev		Serial Communication handle
 * @param ods_config	Structure to return configuration in
 */
int ser4010_get_pa(struct serco *sdev, tPa_Setup *pa_config);

/**
 * Set transmit frequency
 *
 * @param sdev	Serial Communication handle
 * @param freq	Frequency in Hz
 */
int ser4010_set_freq(struct serco *sdev, float freq);

/**
 * Get transmit frequency
 *
 * @param sdev	Serial Communication handle
 * @param freq	Frequency in Hz
 */
int ser4010_get_freq(struct serco *sdev, float *freq);

/**
 * Set FSK frequency deviation
 *
 * Set the frequency deviation for FSK modulation. The actual value that should
 * be used is a magic value that depends on the center frequency, the wanted
 * frequency deviation and a magic lookup table. To calculate this use the
 * si4010_calc_regs_110107.xls spreadsheet.
 *
 * @param sdev	Serial Communication handle
 * @param fdiv	Magic value between 0 and 104 indicating frequency deviation
 */
int ser4010_set_fdiv(struct serco *sdev, float fdiv);

/**
 * Get FSK frequency deviation
 *
 * Get the frequency deviation for FSK modulation. The actual value that should
 * be used is a magic value that depends on the center frequency, the wanted
 * frequency deviation and a magic lookup table. To calculate this use the
 * si4010_calc_regs_110107.xls spreadsheet.
 *
 * @param sdev	Serial Communication handle
 * @param fdiv	Magic value between 0 and 104 indicating frequency deviation
 */
int ser4010_get_fdiv(struct serco *sdev, float *fdiv);

/**
 * Set data encoding to use
 *
 * Set the data encoding to use for encoding the data before sending.
 * bEnc_NoneNrz_c means no encoding is used.
 *
 * @param sdev	Serial Communication handle
 * @param enc	Encoding to use
 */
int ser4010_set_enc(struct serco *sdev, enum Ser4010Encoding enc);

/**
 * Get data encoding used
 *
 * Get the data encoding that is use for encoding the data before sending.
 * bEnc_NoneNrz_c means no encoding is used.
 *
 * @param sdev	Serial Communication handle
 * @param enc	Encoding to use
 */
int ser4010_get_enc(struct serco *sdev, enum Ser4010Encoding *enc);

/**
 * Reconfigure the RF module
 *
 * Trigger a reconfiguration of the RF blocks. This must always be called after
 * changing the RF config to activate the changes.
 *
 * @param sdev	Serial Communication handle
 */
int ser4010_reconfigure(struct serco *sdev);

/**
 * Load frame data
 *
 * Load the frame data to send. Every byte in the frame is send LSB first. Only
 * the first (tOds_Setup.bGroupWidth + 1) bits of a byte will be used.
 *
 * @param sdev	Serial Communication handle
 * @param data	Frame data
 * @param len	Frame data length in bytes
 *
 * @returns TODO:
 */
int ser4010_load_frame(struct serco *sdev, uint8_t *data, size_t len);

/**
 * Send a frame
 *
 * Send the currently loaded frame once or multiple times.
 *
 * @param sdev	Serial Communication handle
 * @param cnt	Number of times to send the frame
 *
 * @returns TODO:
 */
int ser4010_send(struct serco *sdev, unsigned int cnt);

#endif // __SER4010_H__
