/**********************************************************************
 Copyright (c) 1991 MPEG/audio software simulation group, All Rights Reserved
 huffman.h
 **********************************************************************/
/**********************************************************************
 * MPEG/audio coding/decoding software, work in progress              *
 *   NOT for public distribution until verified and approved by the   *
 *   MPEG/audio committee.  For further information, please contact   *
 *   Chad Fogg email: <cfogg@xenon.com>                               *
 *                                                                    *
 * VERSION 4.1                                                        *
 *   changes made since last update:                                  *
 *   date   programmers                comment                        *
 *  27.2.92 F.O.Witte (ITT Intermetall)				      *
 *  8/24/93 M. Iwadare          Changed for 1 pass decoding.          *
 *  7/14/94 J. Koller		useless 'typedef' before huffcodetab  *
 *				removed				      *
 *********************************************************************/

#ifndef mp3Decoder_huffman_h
#define mp3Decoder_huffman_h

#include "audio_data_buf.h"

#define HUFFBITS unsigned long int
#define HTN	34

#define T huffcodetab
typedef struct T *T;

struct T {
    char tablename[3];//string, containing table_description
    unsigned int xlen;//max. x-index+
    unsigned int ylen;//max. y-index+
    unsigned int linbits;//number of linbits
    unsigned int linmax;//max number to be stored in linbits
    int ref;//a positive value indicates a reference
    HUFFBITS *table;//pointer to array[xlen][ylen]
    unsigned char *hlen;//pointer to array[xlen][ylen]
    unsigned char(*val)[2];//decoder tree
    unsigned int treelen;//length of decoder tree
};

extern struct T ht[HTN];

extern int read_decoder_table(FILE *fi);
extern int huffman_decoder(T h, int *x, int *y, int *v, int *w, audio_data_buf buf);

#undef T

#endif
