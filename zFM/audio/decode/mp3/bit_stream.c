//
//  bit_stream.c
//  mp3
//
//  Created by zykhbl on 16/3/10.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "common.h"

#include "bit_stream.h"

//open and initialize the buffer
void alloc_buffer(struct bit_stream *bs, int size) {
    bs->buf = (unsigned char *) mem_alloc(size * sizeof(unsigned char), "buffer");
    bs->buf_size = size;
}

void desalloc_buffer(struct bit_stream *bs) {
    free(bs->buf);
}

//open the device to read the bit stream from it
void open_bit_stream_r(struct bit_stream *bs, char *bs_filenam, int size) {
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

void close_bit_stream_r(struct bit_stream *bs) {
    fclose(bs->pt);
    desalloc_buffer(bs);
}

struct bit_stream *create_bit_stream(char *bs_filenam, int size) {
    struct bit_stream *t;
    t = (struct bit_stream *)mem_alloc((long) sizeof(*t), "Bit_stream_struc");
    
    open_bit_stream_r(t, bs_filenam, size);
    
    return t;
}

void free_bit_stream(struct bit_stream **bs) {
    close_bit_stream_r(*bs);
    
    free(*bs);
    *bs = NULL;
}

//return the status of the bit stream
//returns 1 if end of bit stream was reached
//returns 0 if end of bit stream was not reached
int end_bs(struct bit_stream *bs) {
    return bs->eobs;
}

//return the current bit stream length (in bits)
unsigned long sstell(struct bit_stream *bs) {
    return bs->totbit;
}

void will_refill_buffer(struct bit_stream *bs) {
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
unsigned int get1bit(struct bit_stream *bs) {
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
unsigned long getbits(struct bit_stream *bs, int N) {
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
