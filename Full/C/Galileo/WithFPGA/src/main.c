/* ********************************************************************************************* */
/* * Signature generator                                                                       * */
/* * Authors:                                                                                  * */
/* *     André Bannwart Perina                                                                 * */
/* *     Luciano Falqueto                                                                      * */
/* *     Wallison de Oliveira                                                                  * */
/* ********************************************************************************************* */
/* * Copyright (c) 2016 André B. Perina, Luciano Falqueto and Wallison de Oliveira             * */
/* *                                                                                           * */
/* * Permission is hereby granted, free of charge, to any person obtaining a copy of this      * */
/* * software and associated documentation files (the "Software"), to deal in the Software     * */
/* * without restriction, including without limitation the rights to use, copy, modify,        * */
/* * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to        * */
/* * permit persons to whom the Software is furnished to do so, subject to the following       * */
/* * conditions:                                                                               * */
/* *                                                                                           * */
/* * The above copyright notice and this permission notice shall be included in all copies     * */
/* * or substantial portions of the Software.                                                  * */
/* *                                                                                           * */
/* * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,       * */
/* * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR  * */
/* * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE * */
/* * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      * */
/* * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER    * */
/* * DEALINGS IN THE SOFTWARE.                                                                 * */
/* ********************************************************************************************* */

#include <mraa/aio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "../include/crypt.h"

#define MSG_LEN 32
#define ITERS 128

int main(void) {
	int i, j;
	struct timeval then, now;
	suseconds_t total = 0; 
	FILE *opf;
	mraa_aio_context aio0;
	crypt_context_t context;
	char readings[MSG_LEN + 1];
	char encBuff[32];

	opf = fopen("data.out", "w");
	aio0 = mraa_aio_init(0);
	crypt_initialise(&context);
	/* For test purposes, the key is left wide open here */
	crypt_set_key(&context, "abcdefghijklmnop");

	/* Wait for FPGA to be programmed or reset to clean any trash that RPi may have sent to the FPGA */
	printf("Program or reset FPGA and press any key...");
	getchar();

	for(i = 0; i < MSG_LEN; i++)
		readings[i] = 0;

	for(i = 0; i < ITERS; i++) {
		/* Acquire data from analog input 0 */
		sprintf(&readings[0], "%04x", mraa_aio_read(aio0));
		sprintf(&readings[4], "%04x", mraa_aio_read(aio0));
		sprintf(&readings[8], "%04x", mraa_aio_read(aio0));
		sprintf(&readings[12], "%04x", mraa_aio_read(aio0));
		sprintf(&readings[16], "%04x", mraa_aio_read(aio0));
		sprintf(&readings[20], "%04x", mraa_aio_read(aio0));
		sprintf(&readings[24], "%04x", mraa_aio_read(aio0));
		sprintf(&readings[28], "%04x", mraa_aio_read(aio0));
		//sprintf(readings, "abcdefghijklmnopqrstuvwxyz012345");

		/* Sign data */
		gettimeofday(&then, NULL);
		/* NOTE: iniVector is hardcoded inside hardware. This IV string is useless */
		crypt_sha_aes_sign(&context, readings, encBuff, MSG_LEN, "aaaabbbbccccdddd");
		gettimeofday(&now, NULL);
		total += (now.tv_usec - then.tv_usec);

		/* Save findings to file */
		fprintf(opf, "%s\n", readings);
		for(j = 0; j < 32; j++)
			fprintf(opf, "%02x", encBuff[j] & 0xff);
		fprintf(opf, "\n");
	}

	/* Print statistics */
	printf("Done. Elapsed time: %ld us\n", total);
	printf("Done. Elapsed time per iter: %ld us\n", total / ITERS);

	crypt_terminate(&context);
	mraa_aio_close(aio0);
	fclose(opf);

	return 0;
}
