/* ********************************************************************************************* */
/* * Simple Cryptography Library (HARDWARE ENABLED)                                            * */
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

#include "../include/common.h"
#include "../include/crypt.h"

#include <bcm2835.h>
#include <gcrypt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Initialise a context.
 */
int crypt_initialise(crypt_context_t *context) {
	int rv;

	ASSERT(context, rv, CRYPT_FAILED, "crypt_initialise: Argument is NULL.\n");

	gcry_check_version(GCRYPT_VERSION);
	gcry_control(GCRYCTL_DISABLE_SECMEM);
	gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

	ASSERT(bcm2835_init(), rv, CRYPT_FAILED, "crypt_initialise: bcm2835_init failed.\n");
	ASSERT(bcm2835_spi_begin(), rv, CRYPT_FAILED, "crypt_initialise: bcm2835_spi_begin failed.\n");
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_8);

	/* Set initialised */
	context->initialised = true;
	context->secretKey[0] = '\0';

_err:
	return rv;
}

/**
 * @brief Set secret key.
 */
int crypt_set_key(crypt_context_t *context, char *secretKey) {
	int rv;

	ASSERT(context, rv, CRYPT_FAILED, "crypt_set_key: Argument is NULL.\n");
	ASSERT(secretKey, rv, CRYPT_FAILED, "crypt_set_key: Argument is NULL.\n");
	ASSERT(context->initialised, rv, CRYPT_FAILED, "crypt_set_key: Context is not initialised.\n");

	memcpy(context->secretKey, secretKey, 32);

_err:
	return rv;
}

/**
 * @brief Decipher a buffer using AES-256 with CBC.
 */
int crypt_aes_dec(crypt_context_t *context, char *encBuffer, char *outBuffer, unsigned int buffLen, char *iniVector) {
	int rv;

	ASSERT(context, rv, CRYPT_FAILED, "crypt_aes_dec: Argument is NULL.\n");
	ASSERT(encBuffer, rv, CRYPT_FAILED, "crypt_aes_dec: Argument is NULL.\n");
	ASSERT(outBuffer, rv, CRYPT_FAILED, "crypt_aes_dec: Argument is NULL.\n");
	ASSERT(iniVector, rv, CRYPT_FAILED, "crypt_aes_dec: Argument is NULL.\n");
	ASSERT(context->initialised, rv, CRYPT_FAILED, "crypt_aes_dec: Context is not initialised.\n");

	gcry_error_t gcryError;
	gcry_cipher_hd_t gcryCipherHd = NULL;
	size_t keyLength = gcry_cipher_get_algo_keylen(GCRY_CIPHER_AES256);
	size_t blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER_AES256);

	gcryError = gcry_cipher_open(&gcryCipherHd, GCRY_CIPHER_AES256, GCRY_CIPHER_MODE_CBC, 0);
	ASSERT(!gcryError, rv, CRYPT_FAILED, "crypt_aes_dec: %s: %s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));

	gcryError = gcry_cipher_setkey(gcryCipherHd, context->secretKey, keyLength);
	ASSERT(!gcryError, rv, CRYPT_FAILED, "crypt_aes_dec: %s: %s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));

	gcryError = gcry_cipher_setiv(gcryCipherHd, iniVector, blkLength);
	ASSERT(!gcryError, rv, CRYPT_FAILED, "crypt_aes_dec: %s: %s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));

	gcryError = gcry_cipher_decrypt(gcryCipherHd, outBuffer, buffLen, encBuffer, buffLen);
	ASSERT(!gcryError, rv, CRYPT_FAILED, "crypt_aes_dec: %s: %s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));

_err:
	if(gcryCipherHd)
		gcry_cipher_close(gcryCipherHd);

	return rv;
}

/**
 * @brief Digest a buffer using SHA-256 and cipher using AES-128 with CBC.
 */
int crypt_sha_aes_sign(crypt_context_t *context, char *inBuffer, char *encBuffer, unsigned int buffLen, char *iniVector) {
	int rv;
	char writeData[74];
	char readData[74];

	ASSERT(context, rv, CRYPT_FAILED, "crypt_sha_aes_sign: Argument is NULL.\n");
	ASSERT(inBuffer, rv, CRYPT_FAILED, "crypt_sha_aes_sign: Argument is NULL.\n");
	ASSERT(encBuffer, rv, CRYPT_FAILED, "crypt_sha_aes_sign: Argument is NULL.\n");
	ASSERT(iniVector, rv, CRYPT_FAILED, "crypt_sha_aes_sign: Argument is NULL.\n");
	ASSERT(context->initialised, rv, CRYPT_FAILED, "crypt_sha_aes_sign: Context is not initialised.\n");

	/* First 32 bytes: Data to be sent; 10 bytes for delay; Last 32 bytes: Sign */
	memcpy(writeData, inBuffer, 32);
	bcm2835_spi_transfernb(writeData, readData, 74);
	memcpy(encBuffer, &readData[42], 32);

_err:
	return rv;
}

/**
 * @brief Terminate a context.
 */
int crypt_terminate(crypt_context_t *context) {
	int rv;

	ASSERT(context, rv, CRYPT_FAILED, "crypt_terminate: Argument is NULL.\n");
	ASSERT(context->initialised, rv, CRYPT_FAILED, "crypt_terminate: Context is not initialised.\n");

	bcm2835_spi_end();
	bcm2835_close();

	/* Set terminated */
	context->initialised = false;

_err:
	return rv;
}
