//
//  scale_factors.c
//  mp3
//
//  Created by zykhbl on 16/3/9.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include "scale_factors.h"

struct {
    int l[5];
    int s[3];
} sfbtable = {
    {0, 6, 11, 16, 21},
    {0, 6, 12}
};

int slen[2][16] = {
    {0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4},
    {0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3}
};

void III_get_scale_factors(audio_data_buf buf, III_scalefac_t *scalefac, III_side_info si, int gr, int ch, frame fr_ps) {
    int sfb, i, window;
    struct gr_info_s *gr_info = &(si->ch[ch].gr[gr]);
    
    if (gr_info->window_switching_flag && (gr_info->block_type == 2)) {
        if (gr_info->mixed_block_flag) {//Mix block (block type = 2 and mixed block flag = 1)
            
            //576笔频谱值被分为 17 个scale factor频带
            //slen1 表示频带 0 到 10 的scale factor大小；slen2 表示频带 11 到 16 的scale factor大小
            //其中：part2_length = (8 + 3 × 3) × slen1 + (6 × 3) × slen2
            
            for (sfb = 0; sfb < 8; sfb++) {//前面8个频带为 long block
                (*scalefac)[ch].l[sfb] = (int)hgetbits(buf, slen[0][gr_info->scalefac_compress]);
            }
            
            //后面9个频带为short block，每一个频带包含3个窗口（window）
            for (sfb = 3; sfb < 6; sfb++) {
                for (window = 0; window < 3; window++) {
                    (*scalefac)[ch].s[window][sfb] = (int)hgetbits(buf, slen[0][gr_info->scalefac_compress]);
                }
            }
            for (sfb = 6; sfb < 12; sfb++) {
                for (window = 0; window < 3; window++) {
                    (*scalefac)[ch].s[window][sfb] = (int)hgetbits(buf, slen[1][gr_info->scalefac_compress]);
                }
            }
            
            for (sfb = 12, window = 0; window < 3; window++) {//剩余的数组清零
                (*scalefac)[ch].s[window][sfb] = 0;
            }
        } else {//Short block (block type = 2 and mixed block flag = 0)
            
            //576笔频谱值被分为 12 个 scale factor 频带
            //slen1 表示频带 0 到 5 的 scale factor 大小；slen2 表示频带 6 到 11 的 scale factor 大小
            //其中：part2_length = 3 × 6 × slen1 + 3 × 6 × slen2
            
            for (i = 0; i < 2; i++) {
                for (sfb = sfbtable.s[i]; sfb < sfbtable.s[i + 1]; sfb++) {
                    for (window = 0; window < 3; window++) {
                        (*scalefac)[ch].s[window][sfb] = (int)hgetbits(buf, slen[i][gr_info->scalefac_compress]);
                    }
                }
            }
            
            for (sfb = 12, window = 0; window < 3; window++) {//剩余的数组清零
                (*scalefac)[ch].s[window][sfb] = 0;
            }
        }
    } else {//Long block  (block type = 0、 1、 3)
        
        //576笔频谱值被分为 21 个 scale factor 频带
        //slen1 表示频带 0 到 10 的 scale factor 大小；slen2 表示频带 11 到 20 的 scale factor 大小
        //其中：part2_length = 11 × slen1 + 10 × slen2
        
        for (i = 0; i < 4; i++) {
            
            //当 SCFSI 为 0 表示要读取granule0 和 granule1；当为 1 时只须读取granule0；且granule0的信息与granule1共享
            //即：当解码到第二组时，如果 SCFSI 被设定为 1，则第二组的 scale factor 不必计算，可以由第一组中直接获得
            
            if ((si->ch[ch].scfsi[i] == 0) || (gr == 0)) {
                for (sfb = sfbtable.l[i]; sfb < sfbtable.l[i + 1]; sfb++) {
                    (*scalefac)[ch].l[sfb] = (int)hgetbits(buf, slen[(i < 2) ? 0: 1][gr_info->scalefac_compress]);
                }
            }
        }
        
        (*scalefac)[ch].l[22] = 0;
    }
}
