//
//  decode.c
//  mp3Decoder
//
//  Created by zykhbl on 16/2/24.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"
#include "huffman.h"

#include "decode.h"

int huffman_initialized = FALSE;
char *huffdec = "/Users/weidong_wu/mp3/resource/huffdec.txt";

void initialize_huffman() {
    FILE *fi;
    
    if (huffman_initialized) {
        return;
    }
    if (!(fi = openTableFile(huffdec))) {
        printf("Please check huffman table 'huffdec.txt'\n");
        exit(1);
    }
    
    if (fi == NULL) {
        fprintf(stderr,"decoder table open error\n");
        exit(3);
    }
    
    if (read_decoder_table(fi) != HTN) {
        fprintf(stderr,"decoder table read error\n");
        exit(4);
    }
    
    huffman_initialized = TRUE;
}

struct {
    int l[23];
    int s[14];
} sfBandIndex[3] = {
    {
        {0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 52, 62, 74, 90, 110, 134, 162, 196, 238, 288, 342, 418, 576},
        {0, 4, 8, 12, 16, 22, 30, 40, 52, 66, 84, 106, 136, 192}
    },
    {
        {0, 4, 8, 12, 16, 20, 24, 30, 36, 42, 50, 60, 72, 88, 106, 128, 156, 190, 230, 276, 330, 384, 576},
        {0, 4, 8, 12, 16, 22, 28, 38, 50, 64, 80, 100, 126, 192}
    },
    {
        {0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 54, 66, 82, 102, 126, 156, 194, 240, 296, 364, 448, 550, 576},
        {0, 4, 8, 12, 16, 22, 30, 42, 58, 78, 104, 138, 180, 192}
    }
};

void III_hufman_decode(audio_data_buf buf, long int is[SBLIMIT][SSLIMIT], III_side_info si, int ch, int gr, int part2_start, frame fr_ps) {
    int i, x, y;
    int v, w;
    huffcodetab h;
    int region1Start;
    int region2Start;
    
    initialize_huffman();
    
    // ??
    if (((*si).ch[ch].gr[gr].window_switching_flag) && ((*si).ch[ch].gr[gr].block_type == 2)) {//Find region boundary for short block case
        //Region2
        region1Start = 36; //sfb[9 / 3] * 3 = 36
        region2Start = 576;//No Region2 for short block case
    } else {//Find region boundary for long block case
        region1Start = sfBandIndex[fr_ps->header.sampling_frequency].l[(*si).ch[ch].gr[gr].region0_count + 1];//MI
        region2Start = sfBandIndex[fr_ps->header.sampling_frequency].l[(*si).ch[ch].gr[gr].region0_count + (*si).ch[ch].gr[gr].region1_count + 2];//MI
    }
    
    //为了使各组量化频谱系数所需的比特数最少，无噪声编码把一组576个量化频谱系数分成3个region（由低频到高频分别为big_value区，count1区，zero区），每个region一个霍夫曼码书
    //其中：big_value区又分为 子区0，子区1，子区2，使用边信息 III_side_info_t 里不同的 table_select 选择适当的huffman码表
    //码流从右往左，先解big_value区，再解count1区，最后解zero区
    
    for (i = 0; i < (*si).ch[ch].gr[gr].big_values * 2; i += 2) {//Read bigvalues area（big_value区一个huffman码字表示2个量化系数，使用32个huffman表）
        if (i < region1Start) {
            h = &ht[(*si).ch[ch].gr[gr].table_select[0]];
        } else if (i < region2Start) {
            h = &ht[(*si).ch[ch].gr[gr].table_select[1]];
        } else {
            h = &ht[(*si).ch[ch].gr[gr].table_select[2]];
        }
        
        huffman_decoder(h, &x, &y, &v, &w, buf);
        
        is[i / SSLIMIT][i % SSLIMIT] = x;
        is[(i + 1) / SSLIMIT][(i + 1) % SSLIMIT] = y;
    }
    
    //count1区一个huffman码字表示4个量化系数，一共使用了2本码书
    h = &ht[(*si).ch[ch].gr[gr].count1table_select + 32];
    while ((hsstell(buf) < part2_start + (*si).ch[ch].gr[gr].part2_3_length) && (i < SSLIMIT * SBLIMIT)) {//Read count1 area
        huffman_decoder(h, &x, &y, &v, &w, buf);
        
        is[i / SSLIMIT][i % SSLIMIT] = v;
        is[(i + 1) / SSLIMIT][(i + 1) % SSLIMIT] = w;
        is[(i + 2) / SSLIMIT][(i + 2) % SSLIMIT] = x;
        is[(i + 3) / SSLIMIT][(i + 3) % SSLIMIT] = y;
        i += 4;
    }
    
    if (hsstell(buf) > part2_start + (*si).ch[ch].gr[gr].part2_3_length) {//使用了不属于count1区，而是zero区的位，所以要回退到count1区最后一个可用位的结束位置
        i -= 4;
        rewindNbits(buf, (int)(hsstell(buf) - part2_start - (*si).ch[ch].gr[gr].part2_3_length));
    }
    
    if (hsstell(buf) < part2_start + (*si).ch[ch].gr[gr].part2_3_length) {//Dismiss stuffing Bits（丢弃count1区剩余的位）
        hgetbits(buf, (int)(part2_start + (*si).ch[ch].gr[gr].part2_3_length - hsstell(buf)));
    }
    
    for (; i < SSLIMIT * SBLIMIT; i++) {//zero out rest（zero区不用解码。表示余下子带谱线值全为0）
        is[i / SSLIMIT][i % SSLIMIT] = 0;
    }
}

int pretab[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0};

void III_dequantize_sample(long int is[SBLIMIT][SSLIMIT], double xr[SBLIMIT][SSLIMIT], III_scalefac_t *scalefac, struct gr_info_s *gr_info, int ch, frame fr_ps) {
    int ss, sb, cb = 0, sfreq = fr_ps->header.sampling_frequency;
    int next_cb_boundary, cb_begin = 0, cb_width = 0, sign;
    
    //choose correct scalefactor band per block type, initalize boundary
    if (gr_info->window_switching_flag && (gr_info->block_type == 2)) {
        if (gr_info->mixed_block_flag) {
            next_cb_boundary = sfBandIndex[sfreq].l[1];//Mix block
        } else {
            next_cb_boundary = sfBandIndex[sfreq].s[1] * 3;//pure SHORT block
            cb_width = sfBandIndex[sfreq].s[1];
            cb_begin = 0;
        }
    } else {
            next_cb_boundary = sfBandIndex[sfreq].l[1];//LONG blocks: 0, 1, 3
    }
    
    //apply formula per block type
    for (sb = 0; sb < SBLIMIT; sb++) {
        for (ss = 0; ss < SSLIMIT; ss++) {
            if ((sb * 18) + ss == next_cb_boundary) {//Adjust critical band boundary
                if (gr_info->window_switching_flag && (gr_info->block_type == 2)) {
                    if (gr_info->mixed_block_flag) {
                        if (((sb * 18) + ss) == sfBandIndex[sfreq].l[8]) {
                            next_cb_boundary = sfBandIndex[sfreq].s[4] * 3;
                            cb = 3;
                            cb_width = sfBandIndex[sfreq].s[cb + 1] - sfBandIndex[sfreq].s[cb];
                            cb_begin = sfBandIndex[sfreq].s[cb] * 3;
                        } else if (((sb * 18) + ss) < sfBandIndex[sfreq].l[8]) {
                            next_cb_boundary = sfBandIndex[sfreq].l[(++cb) + 1];
                        } else {
                            next_cb_boundary = sfBandIndex[sfreq].s[(++cb) + 1] * 3;
                            cb_width = sfBandIndex[sfreq].s[cb + 1] - sfBandIndex[sfreq].s[cb];
                            cb_begin = sfBandIndex[sfreq].s[cb] * 3;
                        }
                    } else {
                        next_cb_boundary = sfBandIndex[sfreq].s[(++cb) + 1] * 3;
                        cb_width = sfBandIndex[sfreq].s[cb + 1] - sfBandIndex[sfreq].s[cb];
                        cb_begin = sfBandIndex[sfreq].s[cb] * 3;
                    }
                } else {//long blocks
                    next_cb_boundary = sfBandIndex[sfreq].l[(++cb) + 1];
                }
            }
            
            //Compute overall (global) scaling
            xr[sb][ss] = pow(2.0, (0.25 * (gr_info->global_gain - 210.0)));
            
            //Do long/short dependent scaling operations
            if (gr_info->window_switching_flag && (((gr_info->block_type == 2) && (gr_info->mixed_block_flag == 0)) || ((gr_info->block_type == 2) && gr_info->mixed_block_flag && (sb >= 2)))) {
                xr[sb][ss] *= pow(2.0, 0.25 * -8.0 * gr_info->subblock_gain[(((sb * 18) + ss) - cb_begin) / cb_width]);
                xr[sb][ss] *= pow(2.0, 0.25 * -2.0 * (1.0 + gr_info->scalefac_scale) * (*scalefac)[ch].s[(((sb * 18) + ss) - cb_begin) / cb_width][cb]);
            } else {//LONG block types 0,1,3 & 1st 2 subbands of switched blocks
                xr[sb][ss] *= pow(2.0, -0.5 * (1.0 + gr_info->scalefac_scale) * ((*scalefac)[ch].l[cb] + gr_info->preflag * pretab[cb]));
            }
            
            //Scale quantized value
            //直接超越函数计算，使用 pow 函数，缺点是比较慢；
            //或者使用查表法，用空间换时间，但is的取值范围为 [0, 8191]，需要比较大空间；
            //或者泰勒公式展开，精度越高，计算量越大；
            //或者线性插值法，减少了乘法的计算量，但会有误差；
            //或者使用查表法和线性插值法相结合的综合法：[0, 256]使用查表法，大于256的使用线性插值法，经过统计发现，is的取值小于256的占了99%
            xr[sb][ss] *= pow((double)abs((int)is[sb][ss]), ((double)4.0 / 3.0));
            
            sign = (is[sb][ss] < 0) ? 1 : 0;
            if (sign) {
                xr[sb][ss] = -xr[sb][ss];
            }
        }
    }
}

void III_reorder(double xr[SBLIMIT][SSLIMIT], double ro[SBLIMIT][SSLIMIT], struct gr_info_s *gr_info, frame fr_ps) {
    int sfreq = fr_ps->header.sampling_frequency;
    int sfb, sfb_start, sfb_lines;
    int sb, ss, window, freq, src_line, des_line;
    
    for(sb = 0; sb < SBLIMIT; sb++) {
        for(ss = 0; ss < SSLIMIT; ss++) {
            ro[sb][ss] = 0;
        }
    }
    
    //因为在短窗的huffman编码时，将每个子带内的同一频率的三个窗采样数据均重新排列为同一窗，故在此必须恢复成原来的顺序
    if (gr_info->window_switching_flag && (gr_info->block_type == 2)) {
        if (gr_info->mixed_block_flag) {//Mix block
            for (sb = 0; sb < 2; sb++) {//no reorder for low 2 subbands
                for (ss = 0; ss < SSLIMIT; ss++) {
                    ro[sb][ss] = xr[sb][ss];
                }
            }
            
            //reordering for rest switched short
            for(sfb = 3, sfb_start = sfBandIndex[sfreq].s[3], sfb_lines = sfBandIndex[sfreq].s[4] - sfb_start; sfb < 13; sfb++, sfb_start = sfBandIndex[sfreq].s[sfb], (sfb_lines = sfBandIndex[sfreq].s[sfb + 1] - sfb_start)) {
                for(window = 0; window < 3; window++) {
                    for(freq = 0; freq < sfb_lines; freq++) {
                        src_line = sfb_start * 3 + window * sfb_lines + freq;
                        des_line = (sfb_start * 3) + window + (freq * 3);
                        ro[des_line / SSLIMIT][des_line % SSLIMIT] = xr[src_line / SSLIMIT][src_line % SSLIMIT];
                    }
                }
            }
        } else {//Short block
            for(sfb = 0, sfb_start = 0, sfb_lines = sfBandIndex[sfreq].s[1]; sfb < 13; sfb++, sfb_start = sfBandIndex[sfreq].s[sfb], (sfb_lines = sfBandIndex[sfreq].s[sfb + 1] - sfb_start)) {
                for(window = 0; window < 3; window++) {
                    for(freq = 0; freq < sfb_lines; freq++) {
                        src_line = sfb_start * 3 + window * sfb_lines + freq;
                        des_line = (sfb_start * 3) + window + (freq * 3);
                        ro[des_line / SSLIMIT][des_line % SSLIMIT] = xr[src_line / SSLIMIT][src_line % SSLIMIT];
                    }
                }
            }
        }
    } else {//Long block（而长窗没有重新排列，故不需要再重新排列）
        for (sb = 0; sb < SBLIMIT; sb++) {
            for (ss = 0; ss < SSLIMIT; ss++) {
                ro[sb][ss] = xr[sb][ss];
            }
        }
    }
}

void III_stereo(double xr[2][SBLIMIT][SSLIMIT], double lr[2][SBLIMIT][SSLIMIT], III_scalefac_t *scalefac, struct gr_info_s *gr_info, frame fr_ps) {
    int sfreq = fr_ps->header.sampling_frequency;
    int stereo = fr_ps->stereo;
    int ms_stereo = (fr_ps->header.mode == MPG_MD_JOINT_STEREO) && (fr_ps->header.mode_ext & 0x2);
    int i_stereo = (fr_ps->header.mode == MPG_MD_JOINT_STEREO) && (fr_ps->header.mode_ext & 0x1);
    int sfb;
    int i, j, sb, ss, ch, is_pos[576];
    double is_ratio[576];
    
    for (i = 0; i < 576; i++) {//intialization
        is_pos[i] = 7;
    }
    
    //mp3除了提供单声道及双声道之外，同时还提供强化立体声（intensity stereo）与 MS 立体声这两种立体声的编码方式
    //不过这时候的左右声道就并不是单纯是由反量化所处理过后的值，所以须要经过这个立体声的处理过程来将编码过的立体声信号还原回左/右立体声信号
    
    if ((stereo == 2) && i_stereo) {
        if (gr_info->window_switching_flag && (gr_info->block_type == 2)) {
            if(gr_info->mixed_block_flag) {//Mix block
                int max_sfb = 0;
                
                for (j = 0; j < 3; j++) {
                    int sfbcnt;
                    sfbcnt = 2;
                    for(sfb = 12; sfb >= 3; sfb--) {
                        int lines;
                        lines = sfBandIndex[sfreq].s[sfb + 1] - sfBandIndex[sfreq].s[sfb];
                        i = 3 * sfBandIndex[sfreq].s[sfb] + (j + 1) * lines - 1;
                        while (lines > 0) {
                            if (xr[1][i / SSLIMIT][i % SSLIMIT] != 0.0) {
                                sfbcnt = sfb;
                                sfb = -10;
                                lines = -10;
                            }
                            lines--;
                            i--;
                        }
                    }
                    sfb = sfbcnt + 1;
                    
                    if (sfb > max_sfb) {
                        max_sfb = sfb;
                    }
                    
                    while(sfb < 12) {
                        sb = sfBandIndex[sfreq].s[sfb + 1] - sfBandIndex[sfreq].s[sfb];
                        i = 3 * sfBandIndex[sfreq].s[sfb] + j * sb;
                        for (; sb > 0; sb--) {
                            is_pos[i] = (*scalefac)[1].s[j][sfb];
                            if (is_pos[i] != 7) {
                                is_ratio[i] = tan(is_pos[i] * (PI / 12));
                            }
                            i++;
                        }
                        sfb++;
                    }
                    sb = sfBandIndex[sfreq].s[11] - sfBandIndex[sfreq].s[10];
                    sfb = 3 * sfBandIndex[sfreq].s[10] + j * sb;
                    sb = sfBandIndex[sfreq].s[12] - sfBandIndex[sfreq].s[11];
                    i = 3 * sfBandIndex[sfreq].s[11] + j * sb;
                    for (; sb > 0; sb--) {
                        is_pos[i] = is_pos[sfb];
                        is_ratio[i] = is_ratio[sfb];
                        i++;
                    }
                }
                if (max_sfb <= 3) {
                    i = 2;
                    ss = 17;
                    sb = -1;
                    while (i >= 0) {
                        if (xr[1][i][ss] != 0.0) {
                            sb = i * 18 + ss;
                            i = -1;
                        } else {
                            ss--;
                            if (ss < 0) {
                                i--;
                                ss = 17;
                            }
                        }
                    }
                    i = 0;
                    while (sfBandIndex[sfreq].l[i] <= sb) {
                        i++;
                    }
                    sfb = i;
                    i = sfBandIndex[sfreq].l[i];
                    for (; sfb < 8; sfb++) {
                        sb = sfBandIndex[sfreq].l[sfb + 1] - sfBandIndex[sfreq].l[sfb];
                        for (; sb > 0; sb--) {
                            is_pos[i] = (*scalefac)[1].l[sfb];
                            if (is_pos[i] != 7) {
                                is_ratio[i] = tan(is_pos[i] * (PI / 12));
                            }
                            i++;
                        }
                    }
                }
            } else {//Short block type
                for (j = 0; j < 3; j++) {
                    int sfbcnt;
                    sfbcnt = -1;
                    for(sfb = 12; sfb >= 0; sfb--) {
                        int lines;
                        lines = sfBandIndex[sfreq].s[sfb + 1] - sfBandIndex[sfreq].s[sfb];
                        i = 3 * sfBandIndex[sfreq].s[sfb] + (j + 1) * lines - 1;
                        while (lines > 0) {
                            if (xr[1][i / SSLIMIT][i % SSLIMIT] != 0.0) {
                                sfbcnt = sfb;
                                sfb = -10;
                                lines = -10;
                            }
                            lines--;
                            i--;
                        }
                    }
                    sfb = sfbcnt + 1;
                    while(sfb < 12) {
                        sb = sfBandIndex[sfreq].s[sfb + 1] - sfBandIndex[sfreq].s[sfb];
                        i = 3 * sfBandIndex[sfreq].s[sfb] + j * sb;
                        for (; sb > 0; sb--) {
                            is_pos[i] = (*scalefac)[1].s[j][sfb];
                            if (is_pos[i] != 7) {
                                is_ratio[i] = tan(is_pos[i] * (PI / 12));
                            }
                            i++;
                        }
                        sfb++;
                    }
                    
                    sb = sfBandIndex[sfreq].s[11] - sfBandIndex[sfreq].s[10];
                    sfb = 3 * sfBandIndex[sfreq].s[10] + j * sb;
                    sb = sfBandIndex[sfreq].s[12] - sfBandIndex[sfreq].s[11];
                    i = 3 * sfBandIndex[sfreq].s[11] + j * sb;
                    for (; sb > 0; sb--) {
                        is_pos[i] = is_pos[sfb];
                        is_ratio[i] = is_ratio[sfb];
                        i++;
                    }
                }
            }
        } else {//Long block type
            i = 31;
            ss = 17;
            sb = 0;
            while (i >= 0) {
                if (xr[1][i][ss] != 0.0) {
                    sb = i * 18 + ss;
                    i = -1;
                } else {
                    ss--;
                    if (ss < 0) {
                        i--;
                        ss = 17;
                    }
                }
            }
            i = 0;
            while (sfBandIndex[sfreq].l[i] <= sb) {
                i++;
            }
            sfb = i;
            i = sfBandIndex[sfreq].l[i];
            for (; sfb < 21; sfb++) {
                sb = sfBandIndex[sfreq].l[sfb + 1] - sfBandIndex[sfreq].l[sfb];
                for (; sb > 0; sb--) {
                    is_pos[i] = (*scalefac)[1].l[sfb];
                    if (is_pos[i] != 7) {
                        is_ratio[i] = tan(is_pos[i] * (PI / 12));
                    }
                    i++;
                }
            }
            sfb = sfBandIndex[sfreq].l[20];
            for (sb = 576 - sfBandIndex[sfreq].l[21]; sb > 0; sb--) {
                is_pos[i] = is_pos[sfb];
                is_ratio[i] = is_ratio[sfb];
                i++;
            }
        }
    }
    
    for(ch = 0; ch < 2; ch++) {//初始化所有声道数据为全0
        for(sb = 0; sb < SBLIMIT; sb++) {
            for(ss = 0; ss < SSLIMIT; ss++) {
                lr[ch][sb][ss] = 0;
            }
        }
    }
    
    if (stereo == 2) {//双声道
        for(sb = 0; sb < SBLIMIT; sb++) {
            for(ss = 0; ss < SSLIMIT; ss++) {
                i = (sb * 18) + ss;
                if (is_pos[i] == 7) {
                    if (ms_stereo) {
                        lr[0][sb][ss] = (xr[0][sb][ss] + xr[1][sb][ss]) / 1.41421356;
                        lr[1][sb][ss] = (xr[0][sb][ss] - xr[1][sb][ss]) / 1.41421356;
                    } else {
                        lr[0][sb][ss] = xr[0][sb][ss];
                        lr[1][sb][ss] = xr[1][sb][ss];
                    }
                } else if (i_stereo) {
                    lr[0][sb][ss] = xr[0][sb][ss] * (is_ratio[i] / (1 + is_ratio[i]));
                    lr[1][sb][ss] = xr[0][sb][ss] * (1 / (1 + is_ratio[i]));
                } else {
                    printf("Error in streo processing\n");
                }
            }
        }
    } else {//单声道
        for(sb = 0; sb < SBLIMIT; sb++) {
            for(ss = 0; ss < SSLIMIT; ss++) {
                lr[0][sb][ss] = xr[0][sb][ss];
            }
        }
    }
}

double Ci[8] = {-0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037};

void III_antialias(double xr[SBLIMIT][SSLIMIT], double hybridIn[SBLIMIT][SSLIMIT], struct gr_info_s *gr_info, frame fr_ps) {
    static int    init = 1;
    static double ca[8], cs[8];
    double        bu, bd;//upper and lower butterfly inputs
    int           ss, sb, sblim;
    
    if (init) {
        int i;
        double sq;
        for (i = 0; i < 8; i++) {
            sq = sqrt(1.0 + Ci[i] * Ci[i]);
            cs[i] = 1.0 / sq;
            ca[i] = Ci[i] / sq;
        }
        init = 0;
    }
    
    for(sb = 0; sb < SBLIMIT; sb++) {//clear all inputs
        for(ss = 0; ss < SSLIMIT; ss++) {
            hybridIn[sb][ss] = xr[sb][ss];
        }
    }
    
    if  (gr_info->window_switching_flag && (gr_info->block_type == 2) && !gr_info->mixed_block_flag) {
        return;
    }
    
    //只在长窗口要使用，以减少因互相影响产生的噪声原因：使用长窗框得到较细的频谱分辨率时，同时会有混叠（Aliasing）的产生
    //原始信号被分成 32 个子频带时，在频谱上可见邻近的子频带间有明显的重叠现象，而处于重叠区间的信号将会同时影响两个子频带
    
    if (gr_info->window_switching_flag && gr_info->mixed_block_flag && (gr_info->block_type == 2)) {
        sblim = 1;
    } else {
        sblim = SBLIMIT - 1;
    }
    
    //31 alias-reduction operations between each pair of sub-bands
    //with 8 butterflies between each pair
    for(sb = 0; sb < sblim; sb++) {
        for(ss = 0; ss < 8; ss++) {
            bu = xr[sb][17 - ss];
            bd = xr[sb + 1][ss];
            hybridIn[sb][17 - ss] = (bu * cs[ss]) - (bd * ca[ss]);
            hybridIn[sb + 1][ss] = (bd * cs[ss]) + (bu * ca[ss]);
        }
    }
}

/*------------------------------------------------------------------*/
/*                                                                  */
/*    Function: Calculation of the inverse MDCT                     */
/*    In the case of short blocks the 3 output vectors are already  */
/*    overlapped and added in this modul.                           */
/*                                                                  */
/*    New layer3                                                    */
/*                                                                  */
/*------------------------------------------------------------------*/
void inv_mdct(double in[18], double out[36], int block_type) {
    int     i, m, N, p;
    double  tmp[12], sum;
    static  double  win[4][36];
    static  int init = 0;
    static  double COS[4 * 36];
    
    if(init == 0){
        //type 0
        for(i = 0; i < 36; i++) {
            win[0][i] = sin(PI / 36 * (i + 0.5));
        }
        
        //type 1
        for(i = 0; i < 18; i++) {
            win[1][i] = sin(PI / 36 * (i + 0.5));
        }
        for(i = 18; i < 24; i++) {
            win[1][i] = 1.0;
        }
        for(i = 24; i < 30; i++) {
            win[1][i] = sin(PI / 12 * (i + 0.5 - 18));
        }
        for(i = 30; i < 36; i++) {
            win[1][i] = 0.0;
        }
        
        //type 3
        for(i = 0; i < 6; i++) {
            win[3][i] = 0.0;
        }
        for(i = 6; i < 12; i++) {
            win[3][i] = sin(PI / 12 * (i + 0.5 - 6));
        }
        for(i = 12; i < 18; i++) {
            win[3][i] = 1.0;
        }
        for(i = 18; i < 36; i++) {
            win[3][i] = sin(PI / 36 * (i + 0.5));
        }
        
        //type 2
        for(i = 0; i < 12; i++) {
            win[2][i] = sin(PI / 12 * (i + 0.5));
        }
        for(i = 12; i < 36; i++) {
            win[2][i] = 0.0;
        }
        for (i = 0; i < 4 * 36; i++) {
            COS[i] = cos(PI / (2 * 36) * i);
        }
        
        init++;
    }
    
    for(i = 0; i < 36; i++) {
        out[i] = 0;
    }
    
    if(block_type == 2) {
        N = 12;
        for(i = 0; i < 3; i++) {
            for(p = 0; p < N; p++) {
                sum = 0.0;
                for(m = 0; m < N / 2; m++) {
                    sum += in[i + 3 * m] * cos(PI / (2 * N) * (2 * p + 1 + N / 2) * (2 * m + 1));
                }
                tmp[p] = sum * win[block_type][p] ;
            }
            for(p = 0; p < N; p++) {
                out[6 * i + p + 6] += tmp[p];
            }
        }
    } else {
        N = 36;
        for(p = 0; p < N; p++) {
            sum = 0.0;
            for(m = 0; m < N / 2; m++) {
                sum += in[m] * COS[((2 * p + 1 + N / 2) * (2 * m + 1)) % (4 * 36)];
            }
            out[p] = sum * win[block_type][p];
        }
    }
}

//fsIn:freq samples per subband in
//tsOut:time samples per subband out
void III_hybrid(double fsIn[SSLIMIT], double tsOut[SSLIMIT], int sb, int ch, struct gr_info_s *gr_info, frame fr_ps) {
    int ss;
    double rawout[36];
    static double prevblck[2][SBLIMIT][SSLIMIT];
    static int init = 1;
    int bt;
    
    if (init) {
        int i, j, k;
        
        for(i = 0; i < 2; i++) {
            for(j = 0; j < SBLIMIT; j++) {
                for(k = 0; k < SSLIMIT; k++) {
                    prevblck[i][j][k] = 0.0;
                }
            }
        }
        init = 0;
    }
    
    bt = (gr_info->window_switching_flag && gr_info->mixed_block_flag && (sb < 2)) ? 0 : gr_info->block_type;
    
    inv_mdct(fsIn, rawout, bt);
    
    for(ss = 0; ss < SSLIMIT; ss++) {//overlap addition
        tsOut[ss] = rawout[ss] + prevblck[ch][sb][ss];
        prevblck[ch][sb][ss] = rawout[ss + 18];
    }
}

//Pass the subband sample through the synthesis window
//create in synthesis filter
void create_syn_filter(double filter[64][SBLIMIT]) {
    register int i, k;
    
    for (i = 0; i < 64; i++) {
        for (k = 0; k < 32; k++) {
            if ((filter[i][k] = 1e9 * cos((double)((PI64 * i + PI4) * (2 * k + 1)))) >= 0) {
                modf(filter[i][k] + 0.5, &filter[i][k]);
            } else {
                modf(filter[i][k] - 0.5, &filter[i][k]);
            }
            filter[i][k] *= 1e-9;
        }
    }
}

char *dewindow = "/Users/weidong_wu/mp3/resource/dewindow.txt";

//Window the restored sample
//read in synthesis window
void read_syn_window(double window[HAN_SIZE]) {
    int i, j[4];
    FILE *fp;
    double f[4];
    char t[150];
    
    if (!(fp = openTableFile(dewindow))) {
        printf("Please check synthesis window table 'dewindow.txt'\n");
        exit(1);
    }
    
    for (i = 0; i < 512; i += 4) {
        fgets(t, 150, fp);
        sscanf(t, "D[%d] = %lf D[%d] = %lf D[%d] = %lf D[%d] = %lf\n", j, f, j + 1, f + 1, j + 2, f + 2, j + 3, f + 3);
        if (i == j[0]) {
            window[i] = f[0];
            window[i + 1] = f[1];
            window[i + 2] = f[2];
            window[i + 3] = f[3];
        } else {
            printf("Check index in synthesis window table\n");
            exit(1);
        }
        fgets(t, 150, fp);
    }
    fclose(fp);
}

int subBandSynthesis(double *bandPtr, int channel, short *samples) {
    register int i, j, k;
    register double *bufOffsetPtr, sum;
    static int init = 1;
    typedef double NN[64][32];
    static NN * filter;
    typedef double BB[2][2 * HAN_SIZE];
    static BB *buf;
    static int bufOffset[2] = {64, 64};
    static double *window;
    int clip = 0;//count & return how many samples clipped
    
    if (init) {
        buf = (BB *)mem_alloc(sizeof(BB), "BB");
        filter = (NN *)mem_alloc(sizeof(NN), "NN");
        create_syn_filter(*filter);
        window = (double *)mem_alloc(sizeof(double) * HAN_SIZE, "WIN");
        read_syn_window(window);
        init = 0;
    }
    
    bufOffset[channel] = (bufOffset[channel] - 64) & 0x3ff;
    bufOffsetPtr = &((*buf)[channel][bufOffset[channel]]);
    
    for (i = 0; i < 64; i++) {
        sum = 0;
        for (k = 0; k < 32; k++) {
            sum += bandPtr[k] * (*filter)[i][k];
        }
        bufOffsetPtr[i] = sum;
    }

    for (j = 0; j < 32; j++) {
        sum = 0;
        for (i = 0; i < 16; i++) {
            k = j + (i << 5);
            sum += window[k] * (*buf)[channel][((k + (((i + 1) >> 1) << 6)) + bufOffset[channel]) & 0x3ff];
        }
        {
            long foo = sum * SCALE;
            if (foo >= (long)SCALE) {
                samples[j] = SCALE - 1;
                ++clip;
            } else if (foo < (long)-SCALE) {
                samples[j] = -SCALE;
                ++clip;
            } else {
                samples[j] = foo;
            }
        }
    }
    return clip;
}

