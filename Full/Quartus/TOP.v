/* ********************************************************************************************* */
/* * Top Module                                                                                * */
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

module TOP(
		SYS_CLK,
		PB,
		USER_LED,

		I2C_SCL,
		I2C_SDA,
		GPIO_A
	);

	/* Input clock (50 Mhz) */
	input SYS_CLK;
	/* Push buttons */
	input [4:1] PB;
	/* LEDs */
	output [8:1] USER_LED;

	/* SPI: SCLK */
	input I2C_SCL;
	/* SPI: MOSI */
	input I2C_SDA;
	/* SPI: MISO */
	output GPIO_A;

	wire [255:0] wPMosi;
	wire [255:0] wPMiso;
	wire wPValid;
	wire wShaResetN;
	wire wShaInit;
	wire wShaNext;
	wire wShaMode;
	wire [511:0] wShaBlock;
	wire [255:0] wShaDigest;
	wire wShaDigestValid;
	wire wAesRst;
	wire wAesLd;
	wire wAesDone;
	wire [127:0] wAesTextIn;
	wire [127:0] wAesTextOut;
	wire [2:0] wUserLed;

	assign USER_LED = {5'h1f, wUserLed[2], wUserLed[1], wUserLed[0]};

	/* SPI Slave Module */
	SPISlaveDelayedResponse#(256, 80) spiinst(
		.rst_n(PB[1]),

		.s_sclk(I2C_SCL),
		.s_mosi(I2C_SDA),
		.s_miso(GPIO_A),

		.p_mosi(wPMosi),
		.p_miso(wPMiso),
		.p_valid(wPValid)
	);

	// TODO: Make IV dynamic and transferreable to PC!
	/* Communication, SHA-256 and AES-128 module manager */
	Manager#(128'h61616161626262626363636364646464) manager(
		.clk(SYS_CLK),
		.rst_n(PB[1]),

		.p_mosi(wPMosi),
		.p_miso(wPMiso),
		.p_valid(wPValid),

		.sha_reset_n(wShaResetN),
		.sha_init(wShaInit),
		.sha_next(wShaNext),
		.sha_mode(wShaMode),
		.sha_block(wShaBlock),
		.sha_digest(wShaDigest),
		.sha_digest_valid(wShaDigestValid),

		.aes_rst(wAesRst),
		.aes_ld(wAesLd),
		.aes_done(wAesDone),
		.aes_text_in(wAesTextIn),
		.aes_text_out(wAesTextOut)
	);

	/* SHA-256 Module */
	sha256_core shainst(
		.clk(SYS_CLK),
		.reset_n(wShaResetN),

		.init(wShaInit),
		.next(wShaNext),
		.mode(wShaMode),

		.block(wShaBlock),

		.ready(),
		.digest(wShaDigest),
		.digest_valid(wShaDigestValid)
	);

	/* AES-128 Module */
	aes_cipher_top aesinst(
		.clk(SYS_CLK),
		.rst(wAesRst),

		.ld(wAesLd),
		.done(wAesDone),
		.key(128'h6162636465666768696a6b6c6d6e6f70),
		.text_in(wAesTextIn),
		.text_out(wAesTextOut)
	);

	/* Activity LED for SPI */
	ActivityLED act1(
		.clk(SYS_CLK),
		.rst_n(PB[1]),

		.sig_in(I2C_SCL),
		.led_out(wUserLed[0])
	);

	/* Activity LED for SHA-256 */
	ActivityLED act2(
		.clk(SYS_CLK),
		.rst_n(PB[1]),

		.sig_in(wShaInit),
		.led_out(wUserLed[1])
	);

	/* Activity LED for AES-128 */
	ActivityLED act3(
		.clk(SYS_CLK),
		.rst_n(PB[1]),

		.sig_in(wAesLd),
		.led_out(wUserLed[2])
	);

endmodule
