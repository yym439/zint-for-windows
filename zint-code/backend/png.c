/* png.c - Handles output to PNG file */

/*
    libzint - the open source barcode library
    Copyright (C) 2009-2017 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* vim: set ts=4 sw=4 et : */

#include <stdio.h>
#ifdef _MSC_VER
#include <fcntl.h>
#include <io.h>
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "common.h"

#ifndef NO_PNG
#include <png.h>
#include <zlib.h>
#include <setjmp.h>

#define SSET	"0123456789ABCDEF"

struct mainprog_info_type {
    long width;
    long height;
    FILE *outfile;
    jmp_buf jmpbuf;
};

static void writepng_error_handler(png_structp png_ptr, png_const_charp msg) {
    struct mainprog_info_type *graphic;

    fprintf(stderr, "writepng libpng error: %s (F30)\n", msg);
    fflush(stderr);

    graphic = (struct mainprog_info_type*) png_get_error_ptr(png_ptr);
    if (graphic == NULL) {
        /* we are completely hosed now */
        fprintf(stderr,
                "writepng severe error:  jmpbuf not recoverable; terminating. (F31)\n");
        fflush(stderr);
        return;
    }
    longjmp(graphic->jmpbuf, 1);
}

INTERNAL int png_pixel_plot(struct zint_symbol *symbol, unsigned char *pixelbuf) {
    struct mainprog_info_type wpng_info;
    struct mainprog_info_type *graphic;
    png_structp png_ptr;
    png_infop info_ptr;
    int row, column;
    unsigned char fg[4], bg[4];
    unsigned char white[4] =   { 0xff, 0xff, 0xff, 0xff };
    unsigned char cyan[4] =    {    0, 0xff, 0xff, 0xff };
    unsigned char blue[4] =    {    0,    0, 0xff, 0xff };
    unsigned char magenta[4] = { 0xff,    0, 0xff, 0xff };
    unsigned char red[4] =     { 0xff,    0,    0, 0xff };
    unsigned char yellow[4] =  { 0xff, 0xff,    0, 0xff };
    unsigned char green[4] =   {    0, 0xff,    0, 0xff };
    unsigned char black[4] =   {    0,    0,    0, 0xff };
    unsigned char *map[91] = {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 0x00-0F */
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 0x10-1F */
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 0x20-2F */
        bg, fg, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 0-9 */
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* :;<=>?@ */
        NULL, blue, cyan, NULL, NULL, NULL, green, NULL, NULL, NULL, black, NULL, magenta, /* A-M */
        NULL, NULL, NULL, NULL, red, NULL, NULL, NULL, NULL, white, NULL, yellow, NULL /* N-Z */
    };
    int use_alpha, incr;
    unsigned char *image_data;

#ifndef _MSC_VER
    unsigned char outdata[symbol->bitmap_width * 4];
#else
    unsigned char* outdata = (unsigned char*) _alloca(symbol->bitmap_width * 4);
#endif

    graphic = &wpng_info;

    graphic->width = symbol->bitmap_width;
    graphic->height = symbol->bitmap_height;

    fg[0] = (16 * ctoi(symbol->fgcolour[0])) + ctoi(symbol->fgcolour[1]);
    fg[1] = (16 * ctoi(symbol->fgcolour[2])) + ctoi(symbol->fgcolour[3]);
    fg[2] = (16 * ctoi(symbol->fgcolour[4])) + ctoi(symbol->fgcolour[5]);
    bg[0] = (16 * ctoi(symbol->bgcolour[0])) + ctoi(symbol->bgcolour[1]);
    bg[1] = (16 * ctoi(symbol->bgcolour[2])) + ctoi(symbol->bgcolour[3]);
    bg[2] = (16 * ctoi(symbol->bgcolour[4])) + ctoi(symbol->bgcolour[5]);

    use_alpha = 0;
    
    if (strlen(symbol->fgcolour) > 6) {
        fg[3] = (16 * ctoi(symbol->fgcolour[6])) + ctoi(symbol->fgcolour[7]);
        white[3] = cyan[3] = blue[3] = magenta[3] = red[3] = yellow[3] = green[3] = black[3] = fg[3];
        if (fg[3] != 0xff) use_alpha = 1;
    } else {
        fg[3] = 0xff;
    }
    
    if (strlen(symbol->bgcolour) > 6) {
        bg[3] = (16 * ctoi(symbol->bgcolour[6])) + ctoi(symbol->bgcolour[7]);
        if (bg[3] != 0xff) use_alpha = 1;
    } else {
        bg[3] = 0xff;
    }

    /* Open output file in binary mode */
    if (symbol->output_options & BARCODE_STDOUT) {
#ifdef _MSC_VER
        if (-1 == _setmode(_fileno(stdout), _O_BINARY)) {
            strcpy(symbol->errtxt, "631: Can't open output file");
            return ZINT_ERROR_FILE_ACCESS;
        }
#endif
        graphic->outfile = stdout;
    } else {
        if (!(graphic->outfile = fopen(symbol->outfile, "wb"))) {
            strcpy(symbol->errtxt, "632: Can't open output file");
            return ZINT_ERROR_FILE_ACCESS;
        }
    }

    /* Set up error handling routine as proc() above */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, graphic, writepng_error_handler, NULL);
    if (!png_ptr) {
        strcpy(symbol->errtxt, "633: Out of memory");
        return ZINT_ERROR_MEMORY;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        strcpy(symbol->errtxt, "634: Out of memory");
        return ZINT_ERROR_MEMORY;
    }

    /* catch jumping here */
    if (setjmp(graphic->jmpbuf)) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        strcpy(symbol->errtxt, "635: libpng error occurred");
        return ZINT_ERROR_MEMORY;
    }

    /* open output file with libpng */
    png_init_io(png_ptr, graphic->outfile);

    /* set compression */
    png_set_compression_level(png_ptr, 9);

    /* set Header block */
    if (use_alpha)
        png_set_IHDR(png_ptr, info_ptr, graphic->width, graphic->height,
                8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    else
        png_set_IHDR(png_ptr, info_ptr, graphic->width, graphic->height,
                8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    /* write all chunks up to (but not including) first IDAT */
    png_write_info(png_ptr, info_ptr);

    /* set up the transformations:  for now, just pack low-bit-depth pixels
    into bytes (one, two or four pixels per byte) */
    png_set_packing(png_ptr);

    /* Pixel Plotting */
    incr = use_alpha ? 4 : 3;
    for (row = 0; row < symbol->bitmap_height; row++) {
        unsigned char *pb = pixelbuf + symbol->bitmap_width * row;
        image_data = outdata;
        for (column = 0; column < symbol->bitmap_width; column++, pb++, image_data += incr) {
            memcpy(image_data, map[*pb], incr);
        }
        /* write row contents to file */
        png_write_row(png_ptr, outdata);
    }

    /* End the file */
    png_write_end(png_ptr, NULL);

    /* make sure we have disengaged */
    if (png_ptr && info_ptr) png_destroy_write_struct(&png_ptr, &info_ptr);
    if (symbol->output_options & BARCODE_STDOUT) {
        fflush(wpng_info.outfile);
    } else {
        fclose(wpng_info.outfile);
    }
    return 0;
}
#endif /* NO_PNG */
