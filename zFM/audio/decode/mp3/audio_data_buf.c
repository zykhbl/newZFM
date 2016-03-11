//
//  audio_data_buf.c
//  mp3
//
//  Created by zykhbl on 16/3/10.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include "common.h"

#include "audio_data_buf.h"

#define T audio_data_buf

struct T {
    unsigned int    *buf;
    unsigned int    buf_size;
    unsigned int    buf_bit_idx;
    unsigned long   offset;
    unsigned long   totbit;
    unsigned long   buf_byte_idx;
};

T create_audio_data_buf(int buf_size) {
    T t;
    t = (T)mem_alloc((long) sizeof(*t), "audio_data_buf");
    
    t->buf = (unsigned int *) mem_alloc(buf_size * sizeof(unsigned int), "buffer");
    t->buf_size = buf_size;
    
    t->buf_bit_idx = 8;
    t->totbit = 0;
    t->buf_byte_idx = 0;
    
    return t;
}

void free_audio_data_buf(T *bs) {
    free((*bs)->buf);
    free(*bs);
    *bs = NULL;
}

unsigned int hget_buf_size(T buf) {
    return buf->buf_size;
}

void hputbuf(T buf, unsigned int val, int N) {
    if (N != 8) {
        printf("Not Supported yet!!\n");
        exit(-3);
    }
    buf->buf[buf->offset % buf->buf_size] = val;
    buf->offset++;
}

//return the current bit stream length (in bits)
unsigned long hsstell(T buf) {
    return buf->totbit;
}

//read N bit from the bit stream
unsigned long hgetbits(T buf, int N) {
    unsigned long val = 0;
    register int j = N;
    register int k, tmp;
    
    if (N > MAX_LENGTH) {
        printf("Cannot read or write more than %d bits at a time.\n", MAX_LENGTH);
    }
    
    buf->totbit += N;
    while (j > 0) {
        if (!buf->buf_bit_idx) {
            buf->buf_bit_idx = 8;
            buf->buf_byte_idx++;
            if (buf->buf_byte_idx > buf->offset) {
                printf("Buffer overflow !!\n");
                exit(3);
            }
        }
        k = MIN(j, buf->buf_bit_idx);
        tmp = buf->buf[buf->buf_byte_idx % buf->buf_size] & putmask[buf->buf_bit_idx];
        tmp = tmp >> (buf->buf_bit_idx - k);
        val |= tmp << (j - k);
        buf->buf_bit_idx -= k;
        j -= k;
    }
    return val;
}

unsigned int hget1bit(T buf) {
    return (unsigned int)hgetbits(buf, 1);
}

void rewindNbits(T buf, int N) {
    buf->totbit -= N;
    buf->buf_bit_idx += N;
    while(buf->buf_bit_idx >= 8){
        buf->buf_bit_idx -= 8;
        buf->buf_byte_idx--;
    }
}

void rewindNbytes(T buf, int N) {
    buf->totbit -= N * 8;
    buf->buf_byte_idx -= N;
}

#undef T
