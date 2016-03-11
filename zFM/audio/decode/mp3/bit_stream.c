//
//  bit_stream.c
//  mp3
//
//  Created by zykhbl on 16/3/10.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"

#include "bit_stream.h"

#define T bit_stream

struct T {
    FILE            *pt;            //pointer to bit stream device
    unsigned char   *buf;           //bit stream buffer
    int             buf_size;       //size of buffer (in number of bytes)
    long            totbit;         //bit counter of bit stream
    int             buf_byte_idx;   //pointer to top byte in buffer
    int             buf_bit_idx;    //pointer to top bit of top byte in buffer
    int             mode;           //bit stream open in read or write mode
    int             eob;            //end of buffer index
    int             eobs;           //end of bit stream flag
    char            format;         //format of file in rd mode (BINARY/ASCII)
};

//open and initialize the buffer
void alloc_buffer(T bs, int size) {
    bs->buf = (unsigned char *) mem_alloc(size * sizeof(unsigned char), "buffer");
    bs->buf_size = size;
}

void desalloc_buffer(T bs) {
    free(bs->buf);
}

//open the device to read the bit stream from it
void open_bit_stream_r(T bs, char *bs_filenam, int size) {
    if ((bs->pt = fopen(bs_filenam, "rb")) == NULL) {
        printf("Could not find \"%s\".\n", bs_filenam);
        exit(1);
    }
    
    alloc_buffer(bs, size);
    bs->buf_byte_idx = -1;
    bs->buf_bit_idx = 0;
    bs->totbit = 0;
    bs->mode = READ_MODE;
    bs->eob = FALSE;
    bs->eobs = FALSE;
    bs->format = BINARY;
}

void close_bit_stream_r(T bs) {
    fclose(bs->pt);
    desalloc_buffer(bs);
}

T create_bit_stream(char *bs_filenam, int size) {
    T t;
    t = (T)mem_alloc((long) sizeof(*t), "Bit_stream_struc");
    
    open_bit_stream_r(t, bs_filenam, size);
    
    return t;
}

void free_bit_stream(T *bs) {
    close_bit_stream_r(*bs);
    
    free(*bs);
    *bs = NULL;
}

//return the status of the bit stream
//returns 1 if end of bit stream was reached
//returns 0 if end of bit stream was not reached
int end_bs(T bs) {
    return bs->eobs;
}

//return the current bit stream length (in bits)
unsigned long sstell(T bs) {
    return bs->totbit;
}

void will_refill_buffer(T bs) {
    if (!bs->buf_bit_idx) {
        bs->buf_bit_idx = 8;
        bs->buf_byte_idx++;
        if (bs->buf_byte_idx == bs->eob) {
            if (feof(bs->pt)) {
                bs->eobs = TRUE;
            } else {
                bs->eob = (int)fread(bs->buf, sizeof(unsigned char), bs->buf_size, bs->pt);
                if (feof(bs->pt)) {
                    bs->eobs = TRUE;
                }
                bs->buf_byte_idx = 0;
            }
        }
    }
}

int mask[8] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};

//read 1 bit from the bit stream
unsigned int get1bit(T bs) {
    unsigned int bit;
    
    bs->totbit++;
    
    will_refill_buffer(bs);
    if (bs->eobs) {
        return 0;
    }
    
    bit = bs->buf[bs->buf_byte_idx] & mask[bs->buf_bit_idx - 1];
    bit = bit >> (bs->buf_bit_idx - 1);
    bs->buf_bit_idx--;
    return bit;
}

int putmask[9] = {0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};

//read N bit from the bit stream
unsigned long getbits(T bs, int N) {
    unsigned long val = 0;
    register int j = N;
    register int k, tmp;
    
    if (N > MAX_LENGTH) {
        printf("Cannot read or write more than %d bits at a time.\n", MAX_LENGTH);
    }
    
    bs->totbit += N;
    while (j > 0) {
        will_refill_buffer(bs);
        if (bs->eobs) {
            return 0;
        }
        
        k = MIN(j, bs->buf_bit_idx);
        tmp = bs->buf[bs->buf_byte_idx] & putmask[bs->buf_bit_idx];
        tmp = tmp >> (bs->buf_bit_idx - k);
        val |= tmp << (j - k);
        bs->buf_bit_idx -= k;
        j -= k;
    }
    return val;
}

#undef T
