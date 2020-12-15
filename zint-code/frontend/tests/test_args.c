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

#include <errno.h>
#include "testcommon.h"

static char *exec(const char *cmd, char *buf, int buf_size, int debug, int index) {
    FILE *fp;
    int cnt;

    if (debug & ZINT_DEBUG_TEST_PRINT) printf("%d: %s\n", index, cmd);

    *buf = '\0';

    fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "exec: failed to run '%s'\n", cmd);
        return NULL;
    }
    cnt = fread(buf, 1, buf_size, fp);
    if (fgetc(fp) != EOF) {
        fprintf(stderr, "exec: failed to read full stream (%s)\n", cmd);
        pclose(fp);
        return NULL;
    }
    pclose(fp);

    if (cnt) {
        if (buf[cnt - 1] == '\r' || buf[cnt - 1] == '\n') {
            buf[cnt - 1] = '\0';
            if (buf[cnt - 2] == '\r' || buf[cnt - 2] == '\n') {
                buf[cnt - 2] = '\0';
            }
        }
    }

    return buf;
}

static void arg_int(char *cmd, const char *opt, int val) {
    if (val != -1) {
        sprintf(cmd + (int) strlen(cmd), "%s%s%d", strlen(cmd) ? " " : "", opt, val);
    }
}

static void arg_bool(char *cmd, const char *opt, int val) {
    if (val == 1) {
        sprintf(cmd + (int) strlen(cmd), "%s%s", strlen(cmd) ? " " : "", opt);
    }
}

static void arg_double(char *cmd, const char *opt, double val) {
    if (val != -1) {
        sprintf(cmd + (int) strlen(cmd), "%s%s%g", strlen(cmd) ? " " : "", opt, val);
    }
}

static void arg_data(char *cmd, const char *opt, const char *data) {
    if (data != NULL) {
        sprintf(cmd + (int) strlen(cmd), "%s%s'%s'", strlen(cmd) ? " " : "", opt, data);
    }
}

static int arg_input(char *cmd, const char *filename, const char *input) {
    FILE *fp;
    int cnt;
    if (input != NULL) {
        fp = fopen(filename, "wb");
        if (!fp) {
            fprintf(stderr, "arg_input: failed to open '%s' for writing\n", filename);
            return 0;
        }
        cnt = fwrite(input, 1, strlen(input), fp);
        if (cnt != (int) strlen(input)) {
            fprintf(stderr, "arg_input: failed to write %d bytes, cnt %d written (%s)\n", (int) strlen(input), cnt, filename);
            fclose(fp);
            return 0;
        }
        fclose(fp);
        sprintf(cmd + (int) strlen(cmd), "%s-i '%s'", strlen(cmd) ? " " : "", filename);
        return 1;
    }
    return 0;
}

static void arg_input_mode(char *cmd, int input_mode) {
    if (input_mode != -1) {
        if ((input_mode & 0x07) == DATA_MODE) {
            sprintf(cmd + (int) strlen(cmd), "%s--binary", strlen(cmd) ? " " : "");
        } else if ((input_mode & 0x07) == GS1_MODE) {
            sprintf(cmd + (int) strlen(cmd), "%s--gs1", strlen(cmd) ? " " : "");
        }
        if (input_mode & ESCAPE_MODE) {
            sprintf(cmd + (int) strlen(cmd), "%s--esc", strlen(cmd) ? " " : "");
        }
    }
}

static void arg_output_options(char *cmd, int output_options) {
    if (output_options != -1) {
        if (output_options & BARCODE_BIND) {
            sprintf(cmd + (int) strlen(cmd), "%s--bind", strlen(cmd) ? " " : "");
        }
        if (output_options & BARCODE_BOX) {
            sprintf(cmd + (int) strlen(cmd), "%s--box", strlen(cmd) ? " " : "");
        }
        if (output_options & BARCODE_STDOUT) {
            sprintf(cmd + (int) strlen(cmd), "%s--direct", strlen(cmd) ? " " : "");
        }
        if (output_options & READER_INIT) {
            sprintf(cmd + (int) strlen(cmd), "%s--init", strlen(cmd) ? " " : "");
        }
        if (output_options & SMALL_TEXT) {
            sprintf(cmd + (int) strlen(cmd), "%s--small", strlen(cmd) ? " " : "");
        }
        if (output_options & BOLD_TEXT) {
            sprintf(cmd + (int) strlen(cmd), "%s--bold", strlen(cmd) ? " " : "");
        }
        if (output_options & CMYK_COLOUR) {
            sprintf(cmd + (int) strlen(cmd), "%s--cmyk", strlen(cmd) ? " " : "");
        }
        if (output_options & BARCODE_DOTTY_MODE) {
            sprintf(cmd + (int) strlen(cmd), "%s--dotty", strlen(cmd) ? " " : "");
        }
        if (output_options & GS1_GS_SEPARATOR) {
            sprintf(cmd + (int) strlen(cmd), "%s--gssep", strlen(cmd) ? " " : "");
        }
    }
}

// Tests args that can be detected with `--dump`
static void test_dump_args(int index, int debug) {

    testStart("");

    struct item {
        int b;
        char *data;
        char *data2;
        char *input;
        char *input2;
        int input_mode;
        int output_options;
        int batch;
        int cols;
        int dmre;
        int eci;
        int fullmultibyte;
        int mask;
        int mode;
        char *primary;
        int rows;
        int secure;
        int square;
        int vers;

        char *expected;
    };
    // s/\/\*[ 0-9]*\*\//\=printf("\/*%3d*\/", line(".") - line("'<"))
    struct item data[] = {
        /*  0*/ {              -1, "123", NULL, NULL, NULL,       -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D2 13 9B 39 65 C8 C9 8E B" },
        /*  1*/ { BARCODE_CODE128, "123", NULL, NULL, NULL,       -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D2 13 9B 39 65 C8 C9 8E B" },
        /*  2*/ { BARCODE_CODE128, "123", "456", NULL, NULL,      -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D2 13 9B 39 65 C8 C9 8E B\nD2 19 3B 72 67 4E 4D 8E B" },
        /*  3*/ { BARCODE_CODE128, "123", NULL, NULL, NULL,       -1, -1, 1, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "Warning 141: Can't use batch mode if data given, ignoring\nD2 13 9B 39 65 C8 C9 8E B" },
        /*  4*/ { BARCODE_CODE128, NULL, NULL, "123\n45\n", NULL, -1, -1, 1, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D2 13 9B 39 65 C8 C9 8E B\nD3 97 62 3B 63 AC" },
        /*  5*/ { BARCODE_CODE128, NULL, NULL, "123\n45\n", "7\n",-1, -1, 1, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "Warning 144: Processing first input file 'test_dump_args1.txt' only\nD2 13 9B 39 65 C8 C9 8E B\nD3 97 62 3B 63 AC" },
        /*  6*/ { BARCODE_CODE128, "\t", NULL, NULL, NULL,        -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D0 90 D2 1A 63 AC" },
        /*  7*/ { BARCODE_CODE128, "\\t", NULL, NULL, NULL, ESCAPE_MODE, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D0 90 D2 1A 63 AC" },
        /*  8*/ { BARCODE_CODE128, "123", NULL, NULL, NULL, -1, BARCODE_BIND | BARCODE_BOX | SMALL_TEXT | BOLD_TEXT | CMYK_COLOUR, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D2 13 9B 39 65 C8 C9 8E B" },
        /*  9*/ { BARCODE_CODE128, "123", NULL, NULL, NULL, -1, BARCODE_DOTTY_MODE, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "Error 224: Selected symbology cannot be rendered as dots" },
        /* 10*/ { BARCODE_CODABLOCKF, "ABCDEF", NULL, NULL, NULL, -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D0 97 BA 86 51 88 B1 11 AC 46 D8 C7 58\nD0 97 BB 12 46 88 C5 1A 3C 55 CC C7 58" },
        /* 11*/ { BARCODE_CODABLOCKF, "ABCDEF", NULL, NULL, NULL, -1, -1, 0, 10, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "D0 97 BA 86 51 88 B1 11 AC 44 68 BC 98 EB\nD0 97 BB 12 46 2B BD 7B A3 47 8A 8D 18 EB" },
        /* 12*/ { BARCODE_CODABLOCKF, "ABCDEF", NULL, NULL, NULL, -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL,  3, -1, 0, -1, "D0 97 BA 58 51 88 B1 11 AC 46 36 C7 58\nD0 97 BB 12 46 88 C5 77 AF 74 62 C7 58\nD0 97 BA CE 5D EB DD 1A 3C 56 88 C7 58" },
        /* 13*/ { BARCODE_CODE11, NULL, NULL, "123", NULL,        -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "B2 D6 96 CA B5 6D 64" },
        /* 14*/ { BARCODE_CODE11, NULL, NULL, "123", NULL,        -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0,  1, "B2 D6 96 CA B5 64" },
        /* 15*/ { BARCODE_CODE11, "123", NULL, "456", NULL,       -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0,  2, "B2 D6 96 CA B2\nB2 B6 DA 9A B2" },
        /* 16*/ { BARCODE_CODE11, "123", "456", "789", "012",     -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0,  2, "B2 D6 96 CA B2\nB2 B6 DA 9A B2\nB2 A6 D2 D5 64\nB2 AD AD 2D 64" },
        /* 17*/ { BARCODE_PDF417, "123", NULL, NULL, NULL,        -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL,  1,  0, 0, -1, "FF 54 7A BC 3D 4F 1D 5C 0F E8 A4\nFF 54 7A 90 2F D3 1F AB 8F E8 A4\nFF 54 6A F8 3A BF 15 3C 0F E8 A4\nFF 54 57 9E 24 E7 1A F7 CF E8 A4\nFF 54 7A E7 3D 0D 9D 73 0F E8 A4\nFF 54 7D 70 B9 CB DF 5E CF E8 A4" },
        /* 18*/ { BARCODE_DATAMATRIX, "ABC", NULL, NULL, NULL, -1,          -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "AA 8\nB3 4\n8F 0\nB2 C\nA6 0\nBA C\nD6 0\nEB 4\nE2 8\nFF C" },
        /* 19*/ { BARCODE_DATAMATRIX, "ABC", NULL, NULL, NULL, -1, READER_INIT, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "AA A\nAC 7\n8A 4\nA0 3\nC2 2\nB5 1\n82 2\nBA 7\n8C C\nA0 5\n86 A\nFF F" },
        /* 20*/ { BARCODE_DATAMATRIX, "ABCDEFGHIJK", NULL, NULL, NULL, -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "AA AA AA AA\nA6 C7 FA F9\nB2 AA C7 BA\n98 BF F4 0F\nE8 DA 90 C8\nC7 D5 B6 DF\nC5 50 B0 2C\nFF FF FF FF" },
        /* 21*/ { BARCODE_DATAMATRIX, "ABCDEFGHIJK", NULL, NULL, NULL, -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 1, -1, "AA AA\nA6 D3\nB2 DA\n99 19\nA8 A6\n84 F7\nC0 8C\nF9 87\nFC 4C\nD8 A5\n83 E6\n99 75\nF7 82\nAE 65\n8D 6A\nFF FF" },
        /* 22*/ { BARCODE_DATAMATRIX, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF", NULL, NULL, NULL, -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "AA AA A8\nA6 94 BC\nB2 AD F0\n99 08 F4\nA9 E1 B8\n86 81 CC\nC2 F5 88\nF5 D5 3C\nF2 68 30\nDA 7A BC\nB7 FE 70\nA8 E7 34\n91 40 88\nD6 33 DC\nD2 89 20\nD1 6A 94\nE2 71 A8\nE4 3E EC\nF2 9D 70\nE5 8D FC\nB9 56 50\nFF FF FC" },
        /* 23*/ { BARCODE_DATAMATRIX, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF", NULL, NULL, NULL, -1, -1, 0, -1, 1, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "AA AA AA AA AA AA AA AA\nA6 D9 C8 0B FC 57 F3 17\nB2 BA A7 CA C9 18 87 BE\n99 2F EF 2B F1 A1 B9 DF\nA8 84 99 CA CF 4A BF 14\n86 D5 D9 87 A4 EF F4 9F\n85 44 BF 22 E7 58 C6 8A\nFF FF FF FF FF FF FF FF" },
        /* 24*/ { BARCODE_DATAMATRIX, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEF", NULL, NULL, NULL, -1, -1, 0, -1, 1, -1, 0, -1, -1, NULL, -1, -1, 1, -1, "AA AA A8\nA6 94 BC\nB2 AD F0\n99 08 F4\nA9 E1 B8\n86 81 CC\nC2 F5 88\nF5 D5 3C\nF2 68 30\nDA 7A BC\nB7 FE 70\nA8 E7 34\n91 40 88\nD6 33 DC\nD2 89 20\nD1 6A 94\nE2 71 A8\nE4 3E EC\nF2 9D 70\nE5 8D FC\nB9 56 50\nFF FF FC" },
        /* 25*/ { BARCODE_DATAMATRIX, "[91]12[92]34", NULL, NULL, NULL, GS1_MODE, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "AA A8\nFA 9C\nBC 00\nD7 84\nED E0\nA4 E4\nA7 40\n9D 3C\nBF 50\nFA 24\nB1 68\nE5 04\n92 70\nFF FC" },
        /* 26*/ { BARCODE_DATAMATRIX, "[91]12[92]34", NULL, NULL, NULL, GS1_MODE, GS1_GS_SEPARATOR, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "AA A8\nF9 DC\nBF 20\nD6 C4\nED 10\nA0 0C\nA7 C0\n96 5C\nBA 70\nBB A4\nE2 18\nDD 14\n9C 40\nFF FC" },
        /* 27*/ { BARCODE_DATAMATRIX, "[9\\x31]12[92]34", NULL, NULL, NULL, GS1_MODE | ESCAPE_MODE, GS1_GS_SEPARATOR, 0, -1, 0, -1, 0, -1, -1, NULL, -1, -1, 0, -1, "AA A8\nF9 DC\nBF 20\nD6 C4\nED 10\nA0 0C\nA7 C0\n96 5C\nBA 70\nBB A4\nE2 18\nDD 14\n9C 40\nFF FC" },
        /* 28*/ { BARCODE_EANX_CC, "[91]12", NULL, NULL, NULL,    -1, -1, 0, -1, 0, -1, 0, -1, -1, "12345678+12", -1, -1, 0, -1, "DB BC D3 9C 44 E9 D2 2C 19 E7 A2 D8 A0 00 00 00\nDB 31 1C 9C C7 29 92 47 D9 E9 40 C8 A0 00 00 00\nDA 3B EB 10 AF 09 9A 18 9D 7D 82 E8 A0 00 00 00\n10 00 00 00 00 00 00 00 00 00 00 00 40 00 00 00\n20 00 00 00 00 00 00 00 00 00 00 00 20 00 00 00\n10 00 00 00 00 00 00 00 00 00 00 00 40 00 00 00\n14 68 D1 A6 49 BD 55 C9 D4 22 48 B9 40 59 94 98" },
        /* 29*/ { BARCODE_EANX_CC, "[91]12", NULL, NULL, NULL,    -1, -1, 0, -1, 0, -1, 0, -1,  2, "12345678+12", -1, -1, 0, -1, "D3 A3 E9 DB F5 C9 DB 43 D9 CB 98 D2 20 00 00 00\nD3 25 0F 11 E4 49 D3 51 F1 AC FC D6 20 00 00 00\nD1 33 48 19 39 E9 93 18 49 D8 98 D7 20 00 00 00\nD1 A6 FC DA 1C 49 9B C5 05 E2 84 D7 A0 00 00 00\n10 00 00 00 00 00 00 00 00 00 00 00 40 00 00 00\n20 00 00 00 00 00 00 00 00 00 00 00 20 00 00 00\n10 00 00 00 00 00 00 00 00 00 00 00 40 00 00 00\n14 68 D1 A6 49 BD 55 C9 D4 22 48 B9 40 59 94 98" },
        /* 30*/ { BARCODE_QRCODE, "点", NULL, NULL, NULL,         -1, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1,  1, 0, -1, "FE 2B F8\n82 AA 08\nBA B2 E8\nBA 0A E8\nBA FA E8\n82 E2 08\nFE AB F8\n00 80 00\nD3 3B B0\n60 95 68\n7A B3 A0\n1D 0F 98\nAA D7 30\n00 E6 A8\nFE DA D0\n82 42 20\nBA 0E 38\nBA C7 18\nBA 17 68\n82 B9 40\nFE C5 28" },
        /* 31*/ { BARCODE_QRCODE, "点", NULL, NULL, NULL,         -1, -1, 0, -1, 0, 26, 0, -1, -1, NULL, -1,  1, 0, -1, "FE 5B F8\n82 72 08\nBA DA E8\nBA 52 E8\nBA 2A E8\n82 0A 08\nFE AB F8\n00 D8 00\nEF F6 20\nB5 C2 28\n36 28 88\nFD 42 10\n62 2A C8\n00 95 70\nFE B7 38\n82 FD D8\nBA 97 00\nBA 43 60\nBA C8 C8\n82 C3 68\nFE EA F8" },
        /* 32*/ { BARCODE_QRCODE, "\223\137", NULL, NULL, NULL, DATA_MODE, -1, 0, -1, 0, -1, 0, -1, -1, NULL, -1,  1, 0, -1, "FE 2B F8\n82 0A 08\nBA A2 E8\nBA 0A E8\nBA 5A E8\n82 72 08\nFE AB F8\n00 A0 00\nEF AE 20\n75 B5 20\n82 F7 58\nF4 9D C8\n5E 17 28\n00 C2 20\nFE 88 80\n82 82 38\nBA EA A8\nBA 55 50\nBA D7 68\n82 BD D0\nFE B7 78" },
        /* 33*/ { BARCODE_QRCODE, "\223\137", NULL, NULL, NULL, DATA_MODE, -1, 0, -1, 0, -1, 1, -1, -1, NULL, -1,  1, 0, -1, "FE 2B F8\n82 AA 08\nBA B2 E8\nBA 0A E8\nBA FA E8\n82 E2 08\nFE AB F8\n00 80 00\nD3 3B B0\n60 95 68\n7A B3 A0\n1D 0F 98\nAA D7 30\n00 E6 A8\nFE DA D0\n82 42 20\nBA 0E 38\nBA C7 18\nBA 17 68\n82 B9 40\nFE C5 28" },
        /* 34*/ { BARCODE_QRCODE, "\\x93\\x5F", NULL, NULL, NULL, DATA_MODE | ESCAPE_MODE, -1, 0, -1, 0, -1, 1, -1, -1, NULL, -1,  1, 0, -1, "FE 2B F8\n82 AA 08\nBA B2 E8\nBA 0A E8\nBA FA E8\n82 E2 08\nFE AB F8\n00 80 00\nD3 3B B0\n60 95 68\n7A B3 A0\n1D 0F 98\nAA D7 30\n00 E6 A8\nFE DA D0\n82 42 20\nBA 0E 38\nBA C7 18\nBA 17 68\n82 B9 40\nFE C5 28" },
        /* 35*/ { BARCODE_QRCODE, "点", NULL, NULL, NULL,         -1, -1, 0, -1, 0, -1, 0, 2, -1, NULL, -1,  1, 0, -1, "FE 4B F8\n82 92 08\nBA 42 E8\nBA 92 E8\nBA 3A E8\n82 EA 08\nFE AB F8\n00 38 00\nFB CD 50\nA5 89 18\n0B 74 B8\nFC 81 A0\n92 34 B8\n00 DE 48\nFE AB 10\n82 5E 50\nBA C9 20\nBA C9 20\nBA F4 E0\n82 81 A0\nFE B4 E8" },
        /* 36*/ { BARCODE_HANXIN, "é", NULL, NULL, NULL,  DATA_MODE, -1, 0, -1, 0, -1, 1, -1, -1, NULL, -1, -1, 0, -1, "FE 8A FE\n80 28 02\nBE E8 FA\nA0 94 0A\nAE 3E EA\nAE D2 EA\nAE 74 EA\n00 AA 00\n15 B4 AA\n0B 48 74\nA2 4A A4\nB5 56 2C\nA8 5A A8\n9F 18 50\nAA 07 50\n00 A6 00\nFE 20 EA\n02 C2 EA\nFA C4 EA\n0A 42 0A\nEA 52 FA\nEA 24 02\nEA AA FE" },
        /* 37*/ { BARCODE_HANXIN, "é", NULL, NULL, NULL,  DATA_MODE, -1, 0, -1, 0, -1, 1, 3, -1, NULL, -1, -1, 0, -1, "FE 16 FE\n80 E2 02\nBE C2 FA\nA0 A0 0A\nAE F6 EA\nAE 98 EA\nAE BA EA\n00 E0 00\n15 83 AA\n44 7E AE\n92 9C 78\n25 BF 08\n47 4B 8C\n0D F9 74\nAB E7 50\n00 3A 00\nFE C2 EA\n02 22 EA\nFA DA EA\n0A 22 0A\nEA B2 FA\nEA 9A 02\nEA E8 FE" },
        /* 38*/ { BARCODE_HANXIN, "é", NULL, NULL, NULL,  DATA_MODE, -1, 0, -1, 0, -1, 1, 4, -1, NULL, -1, -1, 0, -1, "FE 8A FE\n80 28 02\nBE E8 FA\nA0 94 0A\nAE 3E EA\nAE D2 EA\nAE 74 EA\n00 AA 00\n15 B4 AA\n0B 48 74\nA2 4A A4\nB5 56 2C\nA8 5A A8\n9F 18 50\nAA 07 50\n00 A6 00\nFE 20 EA\n02 C2 EA\nFA C4 EA\n0A 42 0A\nEA 52 FA\nEA 24 02\nEA AA FE" },
    };
    int data_size = ARRAY_SIZE(data);

    char cmd[4096];
    char buf[4096];

    char *input1_filename = "test_dump_args1.txt";
    char *input2_filename = "test_dump_args2.txt";
    int have_input1;
    int have_input2;

    for (int i = 0; i < data_size; i++) {

        if (index != -1 && i != index) continue;

        strcpy(cmd, "zint --dump");
        if (debug & ZINT_DEBUG_PRINT) {
            strcat(cmd, " --verbose");
        }

        arg_int(cmd, "-b ", data[i].b);
        arg_data(cmd, "-d ", data[i].data);
        arg_data(cmd, "-d ", data[i].data2);
        have_input1 = arg_input(cmd, input1_filename, data[i].input);
        have_input2 = arg_input(cmd, input2_filename, data[i].input2);
        arg_input_mode(cmd, data[i].input_mode);
        arg_output_options(cmd, data[i].output_options);
        arg_bool(cmd, "--batch", data[i].batch);
        arg_int(cmd, "--cols=", data[i].cols);
        arg_bool(cmd, "--dmre", data[i].dmre);
        arg_int(cmd, "--eci=", data[i].eci);
        arg_bool(cmd, "--fullmultibyte", data[i].fullmultibyte);
        arg_int(cmd, "--mask=", data[i].mask);
        arg_int(cmd, "--mode=", data[i].mode);
        arg_data(cmd, "--primary=", data[i].primary);
        arg_int(cmd, "--rows=", data[i].rows);
        arg_int(cmd, "--secure=", data[i].secure);
        arg_bool(cmd, "--square", data[i].square);
        arg_int(cmd, "--vers=", data[i].vers);

        strcat(cmd, " 2>&1");

        assert_nonnull(exec(cmd, buf, sizeof(buf) - 1, debug, i), "i:%d exec(%s) NULL\n", i, cmd);
        assert_zero(strcmp(buf, data[i].expected), "i:%d buf (%s) != expected (%s) (%s)\n", i, buf, data[i].expected, cmd);

        if (have_input1) {
            assert_zero(remove(input1_filename), "i:%d remove(%s) != 0 (%d)\n", i, input1_filename, errno);
        }
        if (have_input2) {
            assert_zero(remove(input2_filename), "i:%d remove(%s) != 0\n", i, input2_filename);
        }
    }

    testFinish();
} 

static void test_input(int index, int debug) {

    testStart("");

    struct item {
        int b;
        int batch;
        int mirror;
        char *filetype;
        char *input;
        char *outfile;

        int num_expected;
        char *expected;
    };
    // s/\/\*[ 0-9]*\*\//\=printf("\/*%3d*\/", line(".") - line("'<"))
    struct item data[] = {
        /*  0*/ { BARCODE_CODE128, 1, 0, NULL, "123\n456\n", "test_batch~.png", 2, "test_batch1.png\000test_batch2.png" },
        /*  1*/ { BARCODE_CODE128, 1, 1, NULL, "123\n456\n7890123456789\n", NULL, 3, "123.png\000456.png\0007890123456789.png" },
        /*  2*/ { BARCODE_CODE128, 1, 1, "svg", "123\n456\n7890123456789\n", NULL, 3, "123.svg\000456.svg\0007890123456789.svg" },
        /*  3*/ { BARCODE_CODE128, 1, 0, NULL, "\n", "test_batch.png", 0, NULL },
        /*  4*/ { BARCODE_CODE128, 1, 0, NULL, "123\n456\n", "test_67890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890~.png", 2, "test_678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901.png\000test_678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678902.png" },
        /*  5*/ { BARCODE_CODE128, 0, 0, "svg", "123", "test_678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901.png", 1, "test_678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901.svg" },
        /*  6*/ { BARCODE_CODE128, 1, 0, "svg", "123\n", "test_678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901.png", 1, "test_678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901.svg" },
        /*  7*/ { BARCODE_CODE128, 1, 0, NULL, "123\n", "test_batch.jpeg", 1, "test_batch.jpeg.png" },
        /*  8*/ { BARCODE_CODE128, 1, 0, NULL, "123\n", "test_batch.jpg", 1, "test_batch.png" },
        /*  9*/ { BARCODE_CODE128, 1, 0, "emf", "123\n", "test_batch.jpeg", 1, "test_batch.jpeg.emf" },
        /* 10*/ { BARCODE_CODE128, 1, 0, "emf", "123\n", "test_batch.jpg", 1, "test_batch.emf" },
        /* 11*/ { BARCODE_CODE128, 1, 0, "eps", "123\n", "test_batch.ps", 1, "test_batch.eps" },
    };
    int data_size = ARRAY_SIZE(data);

    char cmd[4096];
    char buf[4096];

    char *input_filename = "test_input.txt";
    char *outfile;

    for (int i = 0; i < data_size; i++) {

        if (index != -1 && i != index) continue;

        strcpy(cmd, "zint");
        if (debug & ZINT_DEBUG_PRINT) {
            strcat(cmd, " --verbose");
        }

        arg_int(cmd, "-b ", data[i].b);
        arg_bool(cmd, "--batch", data[i].batch);
        arg_bool(cmd, "--mirror", data[i].mirror);
        arg_data(cmd, "--filetype=", data[i].filetype);
        arg_input(cmd, input_filename, data[i].input);
        arg_data(cmd, "-o ", data[i].outfile);

        assert_nonnull(exec(cmd, buf, sizeof(buf) - 1, debug, i), "i:%d exec(%s) NULL\n", i, cmd);

        outfile = data[i].expected;
        for (int j = 0; j < data[i].num_expected; j++) {
            assert_nonzero(testUtilExists(outfile), "i:%d j:%d testUtilExists(%s) != 1\n", i, j, outfile);
            assert_zero(remove(outfile), "i:%d j:%d remove(%s) != 0 (%d)\n", i, j, outfile, errno);
            outfile += strlen(outfile) + 1;
        }

        assert_zero(remove(input_filename), "i:%d remove(%s) != 0 (%d)\n", i, input_filename, errno);
    }

    testFinish();
} 

// Note ordering of `--batch` before/after data/input args affects error messages
static void test_batch_input(int index, int debug) {

    testStart("");

    struct item {
        int b;
        char *data;
        char *input;
        char *input2;

        char *expected;
    };
    // s/\/\*[ 0-9]*\*\//\=printf("\/*%3d*\/", line(".") - line("'<"))
    struct item data[] = {
        /*  0*/ { BARCODE_CODE128, "123", NULL, NULL, "Warning 122: Can't define data in batch mode, ignoring '123'\nWarning 124: No data received, no symbol generated" },
        /*  1*/ { BARCODE_CODE128, "123", "123\n456\n", NULL, "Warning 122: Can't define data in batch mode, ignoring '123'\nD2 13 9B 39 65 C8 C9 8E B\nD2 19 3B 72 67 4E 4D 8E B" },
        /*  3*/ { BARCODE_CODE128, NULL, "123\n456\n", "789\n", "Warning 143: Can only define one input file in batch mode, ignoring 'test_batch_input2.txt'\nD2 13 9B 39 65 C8 C9 8E B\nD2 19 3B 72 67 4E 4D 8E B" },
    };
    int data_size = ARRAY_SIZE(data);

    char cmd[4096];
    char buf[4096];

    char *input1_filename = "test_batch_input1.txt";
    char *input2_filename = "test_batch_input2.txt";
    int have_input1;
    int have_input2;

    for (int i = 0; i < data_size; i++) {

        if (index != -1 && i != index) continue;

        strcpy(cmd, "zint --dump --batch");
        if (debug & ZINT_DEBUG_PRINT) {
            strcat(cmd, " --verbose");
        }

        arg_int(cmd, "-b ", data[i].b);
        arg_data(cmd, "-d ", data[i].data);
        have_input1 = arg_input(cmd, input1_filename, data[i].input);
        have_input2 = arg_input(cmd, input2_filename, data[i].input2);

        strcat(cmd, " 2>&1");

        assert_nonnull(exec(cmd, buf, sizeof(buf) - 1, debug, i), "i:%d exec(%s) NULL\n", i, cmd);
        assert_zero(strcmp(buf, data[i].expected), "i:%d buf (%s) != expected (%s)\n", i, buf, data[i].expected);

        if (have_input1) {
            assert_zero(remove(input1_filename), "i:%d remove(%s) != 0 (%d)\n", i, input1_filename, errno);
        }
        if (have_input2) {
            assert_zero(remove(input2_filename), "i:%d remove(%s) != 0\n", i, input2_filename);
        }
    }

    testFinish();
} 

static void test_batch_large(int index, int debug) {

    testStart("");

    struct item {
        int b;
        int mirror;
        char *pattern;
        int length;

        char *expected;
    };
    // s/\/\*[ 0-9]*\*\//\=printf("\/*%3d*\/", line(".") - line("'<"))
    struct item data[] = {
        /*  0*/ { BARCODE_HANXIN, 0, "1", 7827, "out.png" },
        /*  1*/ { BARCODE_HANXIN, 1, "1", 7827, "11111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111.png" },
        /*  2*/ { BARCODE_HANXIN, 0, "1", 7828, NULL },
    };
    int data_size = ARRAY_SIZE(data);

    char cmd[16384];
    char data_buf[8192];
    char buf[16384];

    char *input_filename = "test_batch_large.txt";
    int have_input;

    for (int i = 0; i < data_size; i++) {

        if (index != -1 && i != index) continue;

        strcpy(cmd, "zint --batch");
        if (debug & ZINT_DEBUG_PRINT) {
            strcat(cmd, " --verbose");
        }

        arg_int(cmd, "-b ", data[i].b);
        arg_bool(cmd, "--mirror", data[i].mirror);

        testUtilStrCpyRepeat(data_buf, data[i].pattern, data[i].length);
        strcat(data_buf, "\n");
        have_input = arg_input(cmd, input_filename, data_buf);

        assert_nonnull(exec(cmd, buf, sizeof(buf) - 1, debug, i), "i:%d exec(%s) NULL\n", i, cmd);
        if (data[i].expected) {
            assert_zero(remove(data[i].expected), "i:%d remove(%s) != 0 (%d)\n", i, data[i].expected, errno);
        } else {
            assert_zero(testUtilExists("out.png"), "i:%d testUtilExists(out.png) != 0 (%d)\n", i, errno);
        }

        if (have_input) {
            assert_zero(remove(input_filename), "i:%d remove(%s) != 0 (%d)\n", i, input_filename, errno);
        }
    }

    testFinish();
} 

static void test_checks(int index, int debug) {

    testStart("");

    struct item {
        int addongap;
        int border;
        int cols;
        double dotsize;
        int eci;
        char *filetype;
        int height;
        int mask;
        int mode;
        int rotate;
        int rows;
        double scale;
        int secure;
        int separator;
        int vers;
        int w;

        char *expected;
    };
    // s/\/\*[ 0-9]*\*\//\=printf("\/*%3d*\/", line(".") - line("'<"))
    struct item data[] = {
        /*  0*/ { -2, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Error 139: Invalid add-on gap value" },
        /*  1*/ {  6, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Warning 140: Invalid add-on gap value" },
        /*  2*/ { -1, -2,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Error 107: Invalid border width value" },
        /*  3*/ { -1, 1001, -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Warning 108: Border width out of range" },
        /*  4*/ { -1, -1,   -1, 0.009, -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Warning 106: Invalid dot radius value" },
        /*  5*/ { -1, -1,   -2, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Error 131: Invalid columns value" },
        /*  6*/ { -1, -1,   68, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Warning 111: Number of columns out of range" },
        /*  7*/ { -1, -1,   -1, -1,    -2,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Error 138: Invalid ECI value" },
        /*  8*/ { -1, -1,   -1, -1,    1000000, NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Warning 118: Invalid ECI code" },
        /*  9*/ { -1, -1,   -1, -1,    -1,      "jpg", -1, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Warning 142: File type 'jpg' not supported, ignoring" },
        /* 10*/ { -1, -1,   -1, -1,    -1,      NULL,  -2, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Error 109: Invalid symbol height value" },
        /* 11*/ { -1, -1,   -1, -1,    -1,      NULL,   0, -1, -1, -1, -1, -1,   -1, -1, -1, -1,   "Warning 110: Symbol height out of range" },
        /* 12*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -2, -1, -1, -1, -1,   -1, -1, -1, -1,   "Error 148: Invalid mask value" },
        /* 13*/ { -1, -1,   -1, -1,    -1,      NULL,  -1,  8, -1, -1, -1, -1,   -1, -1, -1, -1,   "Warning 147: Invalid mask value" },
        /* 14*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1,  7, -1, -1, -1,   -1, -1, -1, -1,   "Warning 116: Invalid mode" },
        /* 15*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -2, -1, -1,   -1, -1, -1, -1,   "Error 117: Invalid rotation value" },
        /* 16*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, 45, -1, -1,   -1, -1, -1, -1,   "Warning 137: Invalid rotation parameter" },
        /* 17*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -2, -1,   -1, -1, -1, -1,   "Error 132: Invalid rows value" },
        /* 18*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, 45, -1,   -1, -1, -1, -1,   "Warning 112: Number of rows out of range" },
        /* 19*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -2,   -1, -1, -1, -1,   "Warning 105: Invalid scale value" },
        /* 20*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, 0.49, -1, -1, -1, -1,   "Warning 146: Scaling less than 0.5 will be set to 0.5 for 'png' output" },
        /* 21*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -2, -1, -1, -1,   "Error 134: Invalid ECC value" },
        /* 22*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,    9, -1, -1, -1,   "Warning 114: ECC level out of range" },
        /* 23*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -2, -1, -1,   "Error 128: Invalid separator value" },
        /* 24*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1,  5, -1, -1,   "Warning 127: Invalid separator value" },
        /* 25*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -2, -1,   "Error 133: Invalid version value" },
        /* 26*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, 85, -1,   "Warning 113: Invalid version" },
        /* 27*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, -2,   "Error 120: Invalid whitespace value '-2'" },
        /* 28*/ { -1, -1,   -1, -1,    -1,      NULL,  -1, -1, -1, -1, -1, -1,   -1, -1, -1, 1001, "Warning 121: Whitespace value out of range" },
    };
    int data_size = ARRAY_SIZE(data);

    char cmd[4096];
    char buf[4096];
    char *outfilename = "out.png";

    for (int i = 0; i < data_size; i++) {

        if (index != -1 && i != index) continue;

        strcpy(cmd, "zint -d '1'");
        if (debug & ZINT_DEBUG_PRINT) {
            strcat(cmd, " --verbose");
        }

        arg_int(cmd, "--addongap=", data[i].addongap);
        arg_int(cmd, "--border=", data[i].border);
        arg_int(cmd, "--cols=", data[i].cols);
        arg_double(cmd, "--dotsize=", data[i].dotsize);
        arg_int(cmd, "--eci=", data[i].eci);
        arg_data(cmd, "--filetype=", data[i].filetype);
        arg_int(cmd, "--height=", data[i].height);
        arg_int(cmd, "--mask=", data[i].mask);
        arg_int(cmd, "--mode=", data[i].mode);
        arg_int(cmd, "--rotate=", data[i].rotate);
        arg_int(cmd, "--rows=", data[i].rows);
        arg_double(cmd, "--scale=", data[i].scale);
        arg_int(cmd, "--secure=", data[i].secure);
        arg_int(cmd, "--separator=", data[i].separator);
        arg_int(cmd, "--vers=", data[i].vers);
        arg_int(cmd, "-w ", data[i].w);

        strcat(cmd, " 2>&1");

        assert_nonnull(exec(cmd, buf, sizeof(buf) - 1, debug, i), "i:%d exec(%s) NULL\n", i, cmd);
        assert_zero(strcmp(buf, data[i].expected), "i:%d buf (%s) != expected (%s)\n", i, buf, data[i].expected);

        if (strncmp(data[i].expected, "Warning", 7) == 0) {
            assert_zero(remove(outfilename), "i:%d remove(%s) != 0 (%d)\n", i, outfilename, errno);
        }
    }

    testFinish();
} 

int main(int argc, char *argv[]) {

    testFunction funcs[] = { /* name, func, has_index, has_generate, has_debug */
        { "test_dump_args", test_dump_args, 1, 0, 1 },
        { "test_input", test_input, 1, 0, 1 },
        { "test_batch_input", test_batch_input, 1, 0, 1 },
        { "test_batch_large", test_batch_large, 1, 0, 1 },
        { "test_checks", test_checks, 1, 0, 1 },
    };

    testRun(argc, argv, funcs, ARRAY_SIZE(funcs));

    testReport();

    return 0;
}
