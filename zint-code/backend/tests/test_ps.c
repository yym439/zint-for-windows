/*
    libzint - the open source barcode library
    Copyright (C) 2020 Robin Stuart <rstuart114@gmail.com>

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

#include "testcommon.h"
#include <sys/stat.h>

static void test_print(int index, int generate, int debug) {

    testStart("");

    int have_ghostscript = testUtilHaveGhostscript();

    int ret;
    struct item {
        int symbology;
        int input_mode;
        int output_options;
        int whitespace_width;
        int option_1;
        int option_2;
        char *fgcolour;
        char *bgcolour;
        char *data;
        char *expected_file;
    };
    struct item data[] = {
        /*  0*/ { BARCODE_CODE128, UNICODE_MODE, BOLD_TEXT, -1, -1, -1, "", "", "Égjpqy", "../data/eps/code128_egrave_bold.eps" },
        /*  1*/ { BARCODE_CODE39, -1, -1, -1, -1, -1, "147AD0", "FC9630", "123", "../data/eps/code39_fg_bg.eps" },
        /*  2*/ { BARCODE_ULTRA, -1, 1, -1, -1, -1, "147AD0", "FC9630", "123", "../data/eps/ultra_fg_bg.eps" },
        /*  3*/ { BARCODE_EANX, -1, -1, -1, -1, -1, "", "", "9771384524017+12", "../data/eps/ean13_2addon_ggs_5.2.2.5.1-2.eps" },
        /*  4*/ { BARCODE_UPCA, -1, -1, -1, -1, -1, "", "", "012345678905+24", "../data/eps/upca_2addon_ggs_5.2.6.6-5.eps" },
        /*  5*/ { BARCODE_UPCE, -1, -1, -1, -1, -1, "", "", "0123456+12345", "../data/eps/upce_5addon.eps" },
        /*  6*/ { BARCODE_UPCE, -1, SMALL_TEXT | BOLD_TEXT, -1, -1, -1, "", "", "0123456+12345", "../data/eps/upce_5addon_small_bold.eps" },
        /*  7*/ { BARCODE_CODE128, UNICODE_MODE, -1, -1, -1, -1, "", "", "A\\B)ç(D", "../data/eps/code128_escape_latin1.eps" },
        /*  8*/ { BARCODE_DBAR_LTD, -1, BOLD_TEXT, -1, -1, -1, "", "", "1501234567890", "../data/eps/dbar_ltd_24724_fig7_bold.eps" },
    };
    int data_size = ARRAY_SIZE(data);

    char *data_dir = "../data/eps";
    char *eps = "out.eps";
    char escaped[1024];
    int escaped_size = 1024;

    if (generate) {
        if (!testUtilExists(data_dir)) {
            ret = mkdir(data_dir, 0755);
            assert_zero(ret, "mkdir(%s) ret %d != 0\n", data_dir, ret);
        }
    }

    for (int i = 0; i < data_size; i++) {

        if (index != -1 && i != index) continue;

        struct zint_symbol *symbol = ZBarcode_Create();
        assert_nonnull(symbol, "Symbol not created\n");

        int length = testUtilSetSymbol(symbol, data[i].symbology, data[i].input_mode, -1 /*eci*/, data[i].option_1, data[i].option_2, -1, data[i].output_options, data[i].data, -1, debug);
        if (data[i].whitespace_width != -1) {
            symbol->whitespace_width = data[i].whitespace_width;
        }
        if (*data[i].fgcolour) {
            strcpy(symbol->fgcolour, data[i].fgcolour);
        }
        if (*data[i].bgcolour) {
            strcpy(symbol->bgcolour, data[i].bgcolour);
        }

        ret = ZBarcode_Encode(symbol, (unsigned char *) data[i].data, length);
        assert_zero(ret, "i:%d %s ZBarcode_Encode ret %d != 0 %s\n", i, testUtilBarcodeName(data[i].symbology), ret, symbol->errtxt);

        strcpy(symbol->outfile, eps);
        ret = ZBarcode_Print(symbol, 0);
        assert_zero(ret, "i:%d %s ZBarcode_Print %s ret %d != 0\n", i, testUtilBarcodeName(data[i].symbology), symbol->outfile, ret);

        if (generate) {
            printf("        /*%3d*/ { %s, %s, %s, %d, %d, %d, \"%s\", \"%s\", \"%s\", \"%s\"},\n",
                    i, testUtilBarcodeName(data[i].symbology), testUtilInputModeName(data[i].input_mode), testUtilOutputOptionsName(data[i].output_options), data[i].whitespace_width,
                    data[i].option_1, data[i].option_2, data[i].fgcolour, data[i].bgcolour, testUtilEscape(data[i].data, length, escaped, escaped_size), data[i].expected_file);
            ret = rename(symbol->outfile, data[i].expected_file);
            assert_zero(ret, "i:%d rename(%s, %s) ret %d != 0\n", i, symbol->outfile, data[i].expected_file, ret);
            if (have_ghostscript) {
                ret = testUtilVerifyGhostscript(data[i].expected_file, debug);
                assert_zero(ret, "i:%d %s ghostscript %s ret %d != 0\n", i, testUtilBarcodeName(data[i].symbology), data[i].expected_file, ret);
            }
        } else {
            assert_nonzero(testUtilExists(symbol->outfile), "i:%d testUtilExists(%s) == 0\n", i, symbol->outfile);
            assert_nonzero(testUtilExists(data[i].expected_file), "i:%d testUtilExists(%s) == 0\n", i, data[i].expected_file);

            ret = testUtilCmpEpss(symbol->outfile, data[i].expected_file);
            assert_zero(ret, "i:%d %s testUtilCmpEpss(%s, %s) %d != 0\n", i, testUtilBarcodeName(data[i].symbology), symbol->outfile, data[i].expected_file, ret);
            assert_zero(remove(symbol->outfile), "i:%d remove(%s) != 0\n", i, symbol->outfile);
        }

        ZBarcode_Delete(symbol);
    }

    testFinish();
}

void ps_convert(const unsigned char *string, unsigned char *ps_string);

static void test_ps_convert(int index) {

    testStart("");

    struct item {
        char *data;
        char *expected;
    };
    struct item data[] = {
        /*  0*/ { "1\\(é)2€3", "1\\\\\\(\351\\)23" },
    };
    int data_size = ARRAY_SIZE(data);

    unsigned char converted[256];

    for (int i = 0; i < data_size; i++) {

        if (index != -1 && i != index) continue;

        ps_convert((unsigned char *) data[i].data, converted);
        assert_zero(strcmp((char *) converted, data[i].expected), "i:%d ps_convert(%s) %s != %s\n", i, data[i].data, converted, data[i].expected);
    }

    testFinish();
}

int main(int argc, char *argv[]) {

    testFunction funcs[] = { /* name, func, has_index, has_generate, has_debug */
        { "test_print", test_print, 1, 1, 1 },
        { "test_ps_convert", test_ps_convert, 1, 0, 0 },
    };

    testRun(argc, argv, funcs, ARRAY_SIZE(funcs));

    testReport();

    return 0;
}
