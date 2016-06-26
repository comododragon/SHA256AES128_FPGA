/* ********************************************************************************************* */
/* * SHA-256 and AES-128 Manager                                                               * */
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

module Manager#(
		// TODO: Make this value dynamic and acquirable by the host
		/* Initialisation vector for AES-128 */
		parameter AES_IV = 'h6162636465666768696a6b6c6d6e6f
	) (
		clk,
		rst_n,

		p_mosi,
		p_miso,
		p_valid,

		sha_reset_n,
		sha_init,
		sha_next,
		sha_mode,
		sha_block,
		sha_digest,
		sha_digest_valid,

		aes_rst,
		aes_ld,
		aes_done,
		aes_text_in,
		aes_text_out
	);

	/* Usual inputs */
	input clk;
	input rst_n;

	/* Parallel bus */
	input [255:0] p_mosi;
	output [255:0] p_miso;
	input p_valid;

	/* IO to/from SHA-256 module */
	output sha_reset_n;
	output sha_init;
	output sha_next;
	output sha_mode;
	output [511:0] sha_block;
	input [255:0] sha_digest;
	input sha_digest_valid;

	/* IO to/from AES-128 module */
	output aes_rst;
	output aes_ld;
	input aes_done;
	output [127:0] aes_text_in;
	input [127:0] aes_text_out;

	reg pValidPrev;
	reg shaResetN;
	reg shaInit;
	reg aesRst;
	reg aesLd;
	reg [127:0] aesTextIn;
	reg [3:0] state;
	reg [127:0] aesTextOutTop;
	reg [127:0] aesTextOutBot;

	assign p_miso = {aesTextOutTop, aesTextOutBot};
	assign sha_reset_n = shaResetN;
	assign sha_init = shaInit;
	assign sha_next = 'b0;
	assign sha_mode = 'b1;
	/* Only 32 bytes are used. The rest is set to standard SHA padding */
	assign sha_block = {p_mosi, 1'b1, 255'h100};
	assign aes_rst = aesRst;
	assign aes_ld = aesLd;
	assign aes_text_in = aesTextIn;

	always @(posedge clk or negedge rst_n) begin
		if(!rst_n) begin
			pValidPrev <= 'b1;
		end
		else begin
			pValidPrev <= p_valid;
		end
	end

	always @(posedge clk or negedge rst_n) begin
		if(!rst_n) begin
			shaResetN <= 'b0;
			shaInit <= 'b0;
			aesRst <= 'b0;
			aesLd <= 'b0;
		end
		else begin
			/* State 1: Start SHA-256 */
			if('h1 == state) begin
				shaInit <= 'b1;
			end
			/* State 2: Deassert start flag for SHA-256 */
			else if('h2 == state) begin
				shaInit <= 'b0;
			end
			/* State 3: Get SHA-256 digest, XOR it with IV, assign half of it to AES block and start AES */
			else if('h3 == state) begin
				aesTextIn <= sha_digest[255:128] ^ AES_IV;
				aesLd <= 'b1;
			end
			/* State 4: When AES is done, assign the other half of digest and start AES */
			else if('h4 == state) begin
				if(aes_done) begin
					aesTextOutTop <= aes_text_out;
					aesTextIn <= sha_digest[127:0] ^ aes_text_out;
					aesLd <= 'b1;
				end
				else begin
					aesLd <= 'b0;
				end
			end
			/* State 5: Save results */
			else if('h5 == state) begin
				if(aes_done) begin
					aesTextOutBot <= aes_text_out;
					shaResetN <= 'b0;
					aesRst <= 'b0;
				end

				aesLd <= 'b0;
			end
			/* State 6: Cleanup and go to idle */
			else if('h6 == state) begin
				shaResetN <= 'b1;
				aesRst <= 'b1;
			end
		end
	end

	always @(posedge clk or negedge rst_n) begin
		if(!rst_n) begin
			state <= 'h6;
		end
		else begin
			/* State 0: Wait for data to be valid in parallel bus */
			if('h0 == state) begin
				if(p_valid && !pValidPrev) begin
					state <= 'h1;
				end
			end
			/* State 1: Start SHA-256 */
			else if('h1 == state) begin
				state <= 'h2;
			end
			/* State 2: Deassert start flag for SHA-256 */
			else if('h2 == state) begin
				if(sha_digest_valid) begin
					state <= 'h3;
				end
			end
			/* State 3: Get SHA-256 digest, XOR it with IV, assign half of it to AES block and start AES */
			else if('h3 == state) begin
				state <= 'h4;
			end
			/* State 4: When AES is done, assign the other half of digest and start AES */
			else if('h4 == state) begin
				if(aes_done) begin
					state <= 'h5;
				end
			end
			/* State 5: Save results */
			else if('h5 == state) begin
				if(aes_done) begin
					state <= 'h6;
				end
			end
			/* Any other state: Cleanup and go to idle */
			else begin
				state <= 'h0;
			end
		end
	end

endmodule
