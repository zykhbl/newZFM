//
//  frame.h
//  mp3
//
//  Created by zykhbl on 16/3/9.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef frame_h
#define frame_h

#include "bit_stream.h"

#define T frame
typedef struct T *T;

typedef struct {//帧头格式:4字节(32位：11111111111...(12个1开头))
    int version;
    int lay;
    int error_protection;
    int bitrate_index;
    int sampling_frequency;
    int padding;
    int extension;
    int mode;
    int mode_ext;
    int copyright;
    int original;
    int emphasis;
} layer;

//Parent Structure Interpreting some Frame Parameters in Header
struct frame {
    layer       header;        //raw header information
    int         actual_mode;    //when writing IS, may forget if 0 chs
    int         stereo;         //1 for mono, 2 for stereo
    int         jsbound;        //first band of joint stereo coding
    int         sblimit;        //total number of sub bands
};

extern T create_frame();
extern void free_frame(T *fr_ps);

extern int seek_sync(bit_stream bs, unsigned long sync, int N);
extern void decode_info(bit_stream bs, T fr_ps);
extern void hdr_to_frps(T fr_ps);
extern void writeHdr(T fr_ps);

extern void buffer_CRC(bit_stream bs, unsigned int *old_crc);
extern int main_data_slots(T fr_ps);

#undef T

#endif /* frame_h */
