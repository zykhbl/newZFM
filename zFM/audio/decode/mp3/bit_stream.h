//
//  bit_stream.h
//  mp3
//
//  Created by zykhbl on 16/3/10.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef bit_stream_h
#define bit_stream_h

#include <stdlib.h>
#include <stdio.h>

struct bit_stream {
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
    
    void            *aq_decoder;
};

extern struct bit_stream *create_bit_stream(char *bs_filenam, int size, void *decoder);
extern void init_bit_stream(struct bit_stream *bs);
extern void free_bit_stream(struct bit_stream **bs);

extern int	end_bs(struct bit_stream *bs);
extern unsigned long sstell(struct bit_stream *bs);
extern unsigned int get1bit(struct bit_stream *bs);
extern unsigned long getbits(struct bit_stream *bs, int N);
extern void seek_bit_stream(struct bit_stream *bs, off_t offset);
extern unsigned long pre_get_version(struct bit_stream *bs);
extern unsigned long pre_get_layer(struct bit_stream *bs);

#endif /* bit_stream_h */
