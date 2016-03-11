//
//  side_info.h
//  mp3
//
//  Created by zykhbl on 16/3/9.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef side_info_h
#define side_info_h

#include "frame.h"

#define T III_side_info
typedef struct T *T;

//Layer III side information
struct III_side_info {
    unsigned main_data_begin;
    unsigned private_bits;
    struct {
        unsigned scfsi[4];
        struct gr_info_s {
            unsigned part2_3_length;
            unsigned big_values;
            unsigned global_gain;
            unsigned scalefac_compress;
            unsigned window_switching_flag;
            unsigned block_type;
            unsigned mixed_block_flag;
            unsigned table_select[3];
            unsigned subblock_gain[3];
            unsigned region0_count;
            unsigned region1_count;
            unsigned preflag;
            unsigned scalefac_scale;
            unsigned count1table_select;
        } gr[2];
    } ch[2];
};

extern T create_III_side_info();
extern void free_III_side_info(T *si);

extern void III_get_side_info(bit_stream bs, T si, frame fr_ps);

#undef T

#endif /* side_info_h */
