/* example for how to use the imxvpuapi decoder interface to decode JPEGs
 * Copyright (C) 2014 Carlos Rafael Giani
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */


#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "imxvpuapi/imxvpuapi_jpeg.h"



/* This is a simple example of how to decode JPEGs with the imxvpuapi library.
 * It reads the given JPEG file and configures the VPU to decode MJPEG data.
 * Then, the decoded pixels are written to the output file, in PPM format.
 *
 * Note that using the JPEG decoder is optional, and it is perfectly OK to use
 * the lower-level video decoder API for JPEGs as well. (In fact, this is what
 * the JPEG decoder does internally.) The JPEG decoder is considerably easier
 * to use, with less boilerplate code, however. */



struct _Context
{
	FILE *fin, *fout;
	ImxVpuJPEGDecoder *jpeg_decoder;
};


Context* init(FILE *input_file, FILE *output_file)
{
	Context *ctx;

	ctx = calloc(1, sizeof(Context));
	ctx->fin = input_file;
	ctx->fout = output_file;

	/* Open the JPEG decoder */
	imx_vpu_jpeg_dec_open(&(ctx->jpeg_decoder), NULL, 0);

	return ctx;
}


Retval run(Context *ctx)
{
	long size;
	void *buf;
	ImxVpuEncodedFrame encoded_frame;
	ImxVpuPicture decoded_picture;
	ImxVpuJPEGInfo info;
	uint8_t *mapped_virtual_address;
	size_t num_out_byte;


	/* Determine size of the input file to be able to read all of its bytes in one go */
	fseek(ctx->fin, 0, SEEK_END);
	size = ftell(ctx->fin);
	fseek(ctx->fin, 0, SEEK_SET);

	/* Allocate buffer for the input data, and read the data into it */
	buf = malloc(size);
	fread(buf, 1, size, ctx->fin);

	/* Setup encoded frame information */
	encoded_frame.data.virtual_address = buf;
	encoded_frame.data_size = size;
	/* Codec data is out-of-band data that is typically stored in a separate space
	 * in containers for each elementary stream; JPEG data does not need it */
	encoded_frame.codec_data = NULL;
	encoded_frame.codec_data_size = 0;

	fprintf(stderr, "encoded input frame:  size: %u byte\n", encoded_frame.data_size);

	/* Perform the actual JPEG decoding */
	ImxVpuDecReturnCodes dec_ret = imx_vpu_jpeg_dec_decode(ctx->jpeg_decoder, &encoded_frame, &decoded_picture);

	if ((dec_ret != IMX_VPU_DEC_RETURN_CODE_OK) || (decoded_picture.framebuffer == NULL))
	{
		if (dec_ret == IMX_VPU_DEC_RETURN_CODE_OK)
			fprintf(stderr, "could not decode this JPEG image : unspecified error (framebuffer is NULL)\n");
		else
			fprintf(stderr, "could not decode this JPEG image : %s\n", imx_vpu_dec_error_string(dec_ret));
		return RETVAL_ERROR;
	}

	/* Get some information about the the frame
	 * Note that the info is only available *after* calling imx_vpu_jpeg_dec_decode() */
	imx_vpu_jpeg_dec_get_info(ctx->jpeg_decoder, &info);
	fprintf(
		stderr,
		"aligned frame size: %u x %u pixel  actual frame size: %u x %u pixel  Y/Cb/Cr stride: %u/%u/%u  Y/Cb/Cr size: %u/%u/%u  Y/Cb/Cr offset: %u/%u/%u  color format: %s\n",
		info.aligned_frame_width, info.aligned_frame_height,
		info.actual_frame_width, info.actual_frame_height,
		info.y_stride, info.cbcr_stride, info.cbcr_stride,
		info.y_size, info.cbcr_size, info.cbcr_size,
		info.y_offset, info.cb_offset, info.cr_offset,
		imx_vpu_color_format_string(info.color_format)
	);

	/* Input data is not needed anymore, so free the input buffer */
	free(buf);

	/* Map the DMA buffer of the decoded picture, write out the decoded pixels, and unmap the buffer again */
	num_out_byte = info.y_size + info.cbcr_size * 2;
	fprintf(stderr, "decoded output picture:  writing %u byte\n", num_out_byte);
	mapped_virtual_address = imx_vpu_dma_buffer_map(decoded_picture.framebuffer->dma_buffer, IMX_VPU_MAPPING_FLAG_READ);
	fwrite(mapped_virtual_address, 1, num_out_byte, ctx->fout);
	imx_vpu_dma_buffer_unmap(decoded_picture.framebuffer->dma_buffer);

	/* Decoded picture is no longer needed, so inform the decoder that it can reclaim it */
	imx_vpu_jpeg_dec_picture_finished(ctx->jpeg_decoder, &decoded_picture);

	return RETVAL_OK;
}


void shutdown(Context *ctx)
{
	/* Shut down the JPEG decoder */
	imx_vpu_jpeg_dec_close(ctx->jpeg_decoder);
}
