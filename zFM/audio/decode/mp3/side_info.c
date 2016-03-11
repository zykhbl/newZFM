//
//  side_info.c
//  mp3
//
//  Created by zykhbl on 16/3/9.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include "common.h"

#include "side_info.h"

#define T III_side_info

T create_III_side_info() {
    T t;
    t = (T)mem_alloc((long) sizeof(*t), "side_info");
    
    return t;
}

void free_III_side_info(T *si) {
    free(*si);
    *si = NULL;
}

void III_get_side_info(bit_stream bs, T si, frame fr_ps) {
    int ch, gr, i;
    int stereo = fr_ps->stereo;
    
    si->main_data_begin = (unsigned)getbits(bs, 9);
    if (stereo == 1) {
        si->private_bits = (unsigned)getbits(bs, 5);
    } else {
        si->private_bits = (unsigned)getbits(bs, 3);
    }
    
    for (ch = 0; ch < stereo; ch++) {
        for (i = 0; i < 4; i++) {
            si->ch[ch].scfsi[i] = get1bit(bs);
        }
    }
    
    for (gr = 0; gr < 2; gr++) {
        for (ch = 0; ch < stereo; ch++) {
            si->ch[ch].gr[gr].part2_3_length = (unsigned)getbits(bs, 12);
            si->ch[ch].gr[gr].big_values = (unsigned)getbits(bs, 9);
            si->ch[ch].gr[gr].global_gain = (unsigned)getbits(bs, 8);
            si->ch[ch].gr[gr].scalefac_compress = (unsigned)getbits(bs, 4);
            si->ch[ch].gr[gr].window_switching_flag = get1bit(bs);
            if (si->ch[ch].gr[gr].window_switching_flag) {
                si->ch[ch].gr[gr].block_type = (unsigned)getbits(bs, 2);
                si->ch[ch].gr[gr].mixed_block_flag = get1bit(bs);
                for (i = 0; i < 2; i++) {
                    si->ch[ch].gr[gr].table_select[i] = (unsigned)getbits(bs, 5);
                }
                for (i = 0; i < 3; i++) {
                    si->ch[ch].gr[gr].subblock_gain[i] = (unsigned)getbits(bs, 3);
                }
                
                if (si->ch[ch].gr[gr].block_type == 0) {//Set region_count parameters since they are implicit in this case.
                    printf("Side info bad: block_type == 0 in split block.\n");
                    exit(0);
                } else if (si->ch[ch].gr[gr].block_type == 2 && si->ch[ch].gr[gr].mixed_block_flag == 0) {
                    si->ch[ch].gr[gr].region0_count = 8;//MI 9;
                } else {
                    si->ch[ch].gr[gr].region0_count = 7;//MI 8;
                }
                si->ch[ch].gr[gr].region1_count = 20 - si->ch[ch].gr[gr].region0_count;
            } else {
                for (i = 0; i < 3; i++) {
                    si->ch[ch].gr[gr].table_select[i] = (unsigned)getbits(bs, 5);
                }
                si->ch[ch].gr[gr].region0_count = (unsigned)getbits(bs, 4);
                si->ch[ch].gr[gr].region1_count = (unsigned)getbits(bs, 3);
                si->ch[ch].gr[gr].block_type = 0;
            }
            si->ch[ch].gr[gr].preflag = get1bit(bs);
            si->ch[ch].gr[gr].scalefac_scale = get1bit(bs);
            si->ch[ch].gr[gr].count1table_select = get1bit(bs);
        }
    }
}

#undef T