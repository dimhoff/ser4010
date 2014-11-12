/**
 * ser4010_main.c - ser4010 firmware main
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
#include <stdlib.h>
#include <string.h>
#include "si4010.h"
#include "si4010_api_rom.h"
#include "soft_uart.h"
#include "serco_defines.h"

#define bool bit
#define true 1
#define false 0

// Transmission parameters
tOds_Setup xdata rOdsSetup;
tPa_Setup xdata  rPaSetup;
BYTE bEnc;
float fFreq;
BYTE bFskDev;

#define bMaxFrameSize_c 256
BYTE xdata abFrameArray[bMaxFrameSize_c];
BYTE bFrameLen;

//-----------------------------------------------------------------------------
//-- ISR
//-----------------------------------------------------------------------------
/**
 * Temperature Sensor Demodulator ISR
 *
 * Interrupt generated when a temperature sample is ready. This function only 
 * calls the correct SI4010 API functions to process the sample.
 */
void vIsr_Dmd(void) interrupt INTERRUPT_DMD using 2
{

	vDmdTs_ClearDmdIntFlag();
	vDmdTs_IsrCall();
}

//-----------------------------------------------------------------------------
//-- Generic helpers
//-----------------------------------------------------------------------------
#define byte_stuff_putc(B) \
	do { \
			if (B == STUFF_BYTE1) \
				ser_putc(STUFF_BYTE1); \
			ser_putc(B); \
	} while (0)

//-----------------------------------------------------------------------------
//-- Radio helpers
//-----------------------------------------------------------------------------

void rf_configure()
{
	// Required bPA_TRIM clearing before calling vPa_Setup() 
	vPa_Setup( &rPaSetup );

	// ODS setup 
	vOds_Setup( &rOdsSetup );

	// Setup the STL encoding for none.
	vStl_EncodeSetup( bEnc, NULL );
}

void rf_init()
{

	// Set DMD interrupt to high priority,
	// any other interrupts have to stay low priority
	PDMD=1;

	// Call the system setup. This just for initialization.
	// Argument of 1 just configures the SYS module such that the
	// bandgap can be turned off if needed.  
	vSys_Setup( 1 );

	// Setup the bandgap for working with the temperature sensor here.
	// bSys_FirstBatteryInsertWaitTime set to non zero value. 
	vSys_BandGapLdo( 1 );

	// Setup and run the frequency casting. 
	vFCast_Setup();

	// Configure RF components
	rf_configure();

	// Disable Bandgap and LDO till needed
	vSys_BandGapLdo(0);
}

void rf_transmit_frame(float freq, BYTE fdev, BYTE xdata *pbFrameHead, BYTE bLen, BYTE cnt)
{
	// Enable the Bandgap and LDO
	vSys_BandGapLdo(1);

	// Configure RF components
	rf_configure();

	// Tune to the right frequency and set FSK ferquency adjust
	vFCast_Tune(freq);
	vFCast_FskAdj(fdev);
	while ( 0 == bDmdTs_GetSamplesTaken() ) {}
	vPa_Tune( iDmdTs_GetLatestTemp() );

	// Run a single TX loop 
	vStl_PreLoop();
	while (cnt != 0) {
		vStl_SingleTxLoop(pbFrameHead, bLen);
		cnt--;
	}
	vStl_PostLoop();

	// Disable Bandgap and LDO to save power
	vSys_BandGapLdo(0);
}

//-----------------------------------------------------------------------------
//-- Main
//-----------------------------------------------------------------------------
void main()
{
	BYTE xdata cmd[256];
	BYTE cmd_len;
	BYTE c;
	bool comm_error;
	bool stuff_first;
	BYTE res;
	BYTE xdata res_buf[256];
	BYTE res_len;
	BYTE i;

	// Set default PA
	// From fcast_demo program:
	// """Zero out Alpha and Beta here. They have to do with the antenna.
	// Chose a nice high PA Level. This value, along with the nominal cap
	// come from the CAL SPREADSHEET"""
	rPaSetup.fAlpha      = 0;
	rPaSetup.fBeta       = 0;
	rPaSetup.bLevel      = 60;
	rPaSetup.wNominalCap = 256;
	rPaSetup.bMaxDrv     = 0;

	// Set default ODS
	rOdsSetup.bModulationType = 0;  // Use OOK
	rOdsSetup.bClkDiv     = 5;
	rOdsSetup.bEdgeRate   = 0;
	rOdsSetup.bGroupWidth = 7;
	rOdsSetup.wBitRate    = 2416;   // Bit width in seconds = (ods_datarate*(ods_ck_div+1))/24MHz
	rOdsSetup.bLcWarmInt  = 8;
	rOdsSetup.bDivWarmInt = 5;
	rOdsSetup.bPaWarmInt  = 4;

	bEnc  = bEnc_NoneNrz_c;

	fFreq = 433.9e6;
	bFskDev = 104;

	// Init various components
	ser_init();
	rf_init();

	// Main loop
	while ( 1 )
	{
		// Receive one data frame on serial bus
		do {
			cmd_len = 0;
			comm_error = false;
			stuff_first = false;

			while (true) {
				c = ser_getc();

				// Remove Byte stuffing and detect end-of-record
				if (stuff_first) {
					stuff_first = false;
					if (c == STUFF_BYTE2) {
						break;
					} else if (c != STUFF_BYTE1) {
						comm_error = true;
						continue;
					}
				} else if (c == STUFF_BYTE1) {
					stuff_first = true;
					continue;
				}
				if (comm_error) {
					continue;
				}
				if (cmd_len >= sizeof(cmd)) {
					comm_error = true;
					continue;
				}

				cmd[cmd_len] = c;
				cmd_len++;
			}
		} while (comm_error);


		// Parse and execute command
		res_len = 0;
		res = STATUS_LOGIC_ERROR;
		if (cmd_len < 2) {
			res = STATUS_INVALID_FRAME_LEN;
		} else {
			switch (cmd[CMD_OPCODE]) {
			case  CMD_NOP:
				res = STATUS_OK;
				break;
			case CMD_DEV_TYPE:
				res_len = 2;
				res_buf[0] = SER4010_DEV_TYPE >> 8;
				res_buf[1] = SER4010_DEV_TYPE & 0xff;

				res = STATUS_OK;
				break;
			case CMD_DEV_REV:
				res_len = 2;
				res_buf[0] = SER4010_DEV_REV >> 8;
				res_buf[1] = SER4010_DEV_REV & 0xff;

				res = STATUS_OK;
				break;
			case CMD_GET_ODS:
				res_len = sizeof(rOdsSetup);
				memcpy(res_buf, &rOdsSetup, res_len);

				res = STATUS_OK;
				break;
			case CMD_SET_ODS:
				if (cmd_len - CMD_PAYLOAD != sizeof(rOdsSetup)) {
					res = STATUS_INVALID_FRAME_LEN;
				} else {
					memcpy(&rOdsSetup, &cmd[CMD_PAYLOAD], sizeof(rOdsSetup));

					res = STATUS_OK;
				}
				break;
			case CMD_GET_PA:
				res_len = sizeof(rPaSetup);
				memcpy(res_buf, &rPaSetup, res_len);

				res = STATUS_OK;
				break;
			case CMD_SET_PA:
				if (cmd_len - CMD_PAYLOAD != sizeof(rPaSetup)) {
					res = STATUS_INVALID_FRAME_LEN;
				} else {
					memcpy(&rPaSetup, &cmd[CMD_PAYLOAD], sizeof(rPaSetup));

					res = STATUS_OK;
				}
				break;
			case CMD_GET_FREQ:
				res_len = sizeof(fFreq);
				memcpy(res_buf, &fFreq, res_len);

				res = STATUS_OK;
				break;
			case CMD_SET_FREQ:
				if (cmd_len - CMD_PAYLOAD != sizeof(fFreq)) {
					res = STATUS_INVALID_FRAME_LEN;
				} else {
					memcpy(&fFreq, &cmd[CMD_PAYLOAD], sizeof(fFreq));

					res = STATUS_OK;
				}
				break;
			case CMD_GET_FDEV:
				res_len = 1;
				res_buf[0] = bFskDev;

				res = STATUS_OK;
				break;
			case CMD_SET_FDEV:
				if (cmd_len - CMD_PAYLOAD != 1) {
					res = STATUS_INVALID_FRAME_LEN;
				} else {
					bFskDev = cmd[CMD_PAYLOAD];

					res = STATUS_OK;
				}
				break;
			case CMD_GET_ENC:
				res_len = 1;
				res_buf[0] = bEnc;

				res = STATUS_OK;
				break;
			case CMD_SET_ENC:
				if (cmd_len - CMD_PAYLOAD != 1) {
					res = STATUS_INVALID_FRAME_LEN;
				} else if (cmd[CMD_PAYLOAD] < 0 || cmd[CMD_PAYLOAD] > 2) {
					res = STATUS_INVALID_ARGUMENT;
				} else {
					bEnc = cmd[CMD_PAYLOAD];

					res = STATUS_OK;
				}
				break;
			case CMD_LOAD_FRAME:
				bFrameLen = cmd_len - 2;
				memcpy(abFrameArray, &cmd[CMD_PAYLOAD], bFrameLen);
				res = STATUS_OK;
				break;
			case CMD_APPEND_FRAME:
				if (bMaxFrameSize_c - bFrameLen < cmd_len - CMD_PAYLOAD) {
					res = STATUS_TOO_MUCH_DATA;
				} else {
					memcpy(&abFrameArray[bFrameLen], &cmd[CMD_PAYLOAD], cmd_len - CMD_PAYLOAD);
					bFrameLen += cmd_len - CMD_PAYLOAD;
					res = STATUS_OK;
				}
				break;
			case CMD_RF_SEND:
				if (cmd_len - CMD_PAYLOAD != 5) {
					res = STATUS_INVALID_FRAME_LEN;
				} else if ( cmd[CMD_PAYLOAD + 0] != SEND_COOKIE_0 ||
							cmd[CMD_PAYLOAD + 1] != SEND_COOKIE_1 ||
							cmd[CMD_PAYLOAD + 2] != SEND_COOKIE_2 ||
							cmd[CMD_PAYLOAD + 3] != SEND_COOKIE_3)
				{
					res = STATUS_INVALID_SEND_COOKIE;
				} else {
					rf_transmit_frame(fFreq, bFskDev, abFrameArray, bFrameLen, cmd[CMD_PAYLOAD+4]);
					res = STATUS_OK;
				}
				break;
			default:
				res = STATUS_UNKNOWN_CMD;
				break;
			}
		}

		// Send Response
		byte_stuff_putc(cmd[CMD_ID]);
		byte_stuff_putc(res);
		for (i=0; i < res_len; i++) {
			byte_stuff_putc(res_buf[i]);
		}
		ser_putc(STUFF_BYTE1);
		ser_putc(STUFF_BYTE2);
	}
}
