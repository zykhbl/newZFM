//
//  frame.c
//  mp3
//
//  Created by zykhbl on 16/3/9.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "common.h"

#include "frame.h"

#define T frame

T create_frame() {
    T t;
    t = (T)mem_alloc((long) sizeof(*t), "frame");
    
    return t;
}

void free_frame(T *fr_ps) {
    free(*fr_ps);
    *fr_ps = NULL;
}

int seek_sync(bit_stream bs, unsigned long sync, int N) {
    unsigned long aligning;
    unsigned long val;
    long maxi = (int)pow(2.0, (double)N) - 1;
    
    aligning = sstell(bs) % ALIGNING;
    if (aligning) {
        getbits(bs, (int)(ALIGNING - aligning));
    }
    
    val = getbits(bs, N);
    while (((val & maxi) != sync) && (!end_bs(bs))) {
        val <<= ALIGNING;
        val |= getbits(bs, ALIGNING);
    }
    
    if (end_bs(bs)) {
        return 0;
    } else {
        return 1;
    }
}

void decode_info(bit_stream bs, T fr_ps) {
    layer *hdr = &fr_ps->header;
    
    hdr->version = get1bit(bs);// 0-保留的; 1-MPEG
    hdr->lay = 4 - (int)getbits(bs, 2); // 00-未定义; 01-Layer 3; 10-Layer 2; 11-Layer 1
    hdr->error_protection = !get1bit(bs); // 0转为TRUE，检验；1转为FALSE，不校验
    hdr->bitrate_index = (int)getbits(bs, 4);
    hdr->sampling_frequency = (int)getbits(bs, 2);
    hdr->padding = get1bit(bs);
    hdr->extension = get1bit(bs);
    hdr->mode = (int)getbits(bs, 2);
    hdr->mode_ext = (int)getbits(bs, 2);
    hdr->copyright = get1bit(bs);
    hdr->original = get1bit(bs);
    hdr->emphasis = (int)getbits(bs, 2);
}

int js_bound(int lay, int m_ext) {
    static int jsb_table[3][4] =  {
        {4, 8, 12, 16},
        {4, 8, 12, 16},
        {0, 4,  8, 16}
    };//lay + m_e->jsbound
    
    if(lay < 1 || lay > 3 || m_ext < 0 || m_ext > 3) {
        fprintf(stderr, "js_bound bad layer/modext (%d/%d)\n", lay, m_ext);
        exit(1);
    }
    return(jsb_table[lay - 1][m_ext]);
}

//interpret data in hdr str to fields in fr_ps
void hdr_to_frps(T fr_ps) {
    layer *hdr = &fr_ps->header;//(or pass in as arg?)
    
    fr_ps->actual_mode = hdr->mode;
    fr_ps->stereo = (hdr->mode == MPG_MD_MONO) ? 1 : 2;
    fr_ps->sblimit = SBLIMIT;
    if(hdr->mode == MPG_MD_JOINT_STEREO) {
        fr_ps->jsbound = js_bound(hdr->lay, hdr->mode_ext);
    } else {
        fr_ps->jsbound = fr_ps->sblimit;
    }
}

char *layer_names[3] = {"I", "II", "III"};

int bitrate[3][15] = {
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
    {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384},
    {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320}
};

double s_freq[4] = {44.1, 48, 32, 0};

char *mode_names[4] = {"stereo", "j-stereo", "dual-ch", "single-ch"};

void writeHdr(T fr_ps) {
    layer *info = &fr_ps->header;
    
    printf("HDR:  sync=FFF, id=%X, layer=%X, ep=%X, br=%X, sf=%X, pd=%X, ", info->version, info->lay, !info->error_protection, info->bitrate_index, info->sampling_frequency, info->padding);
    
    printf("pr=%X, m=%X, js=%X, c=%X, o=%X, e=%X\n", info->extension, info->mode, info->mode_ext, info->copyright, info->original, info->emphasis);
    
    printf("layer=%s, tot bitrate=%d, sfrq=%.1f, mode=%s, ", layer_names[info->lay-1], bitrate[info->lay-1][info->bitrate_index], s_freq[info->sampling_frequency], mode_names[info->mode]);
    
    printf("sblim=%d, jsbd=%d, ch=%d\n", fr_ps->sblimit, fr_ps->jsbound, fr_ps->stereo);
}

void buffer_CRC(bit_stream bs, unsigned int *old_crc) {
    *old_crc = (unsigned int)getbits(bs, 16);
}

int main_data_slots(T fr_ps) {//根据帧头信息，返回当前帧音频主数据的slot个数
    int nSlots;
    
    nSlots = (144 * bitrate[2][fr_ps->header.bitrate_index]) / s_freq[fr_ps->header.sampling_frequency];
    
    if (fr_ps->header.padding) {//如果frame中包含附加slot，以调整平均比特率与采样频率一致，那么加上1字节
        nSlots++;
    }
    
    nSlots -= 4;//减去帧头4字节(32位)
    
    if (fr_ps->header.error_protection) {//如果有错误码校验，再减去2字节(16位)
        nSlots -= 2;
    }
    
    //减去Side信息，Side info大小由声道决定，单声道17字节，双声道32位字节
    if (fr_ps->stereo == 1) {
        nSlots -= 17;
    } else {
        nSlots -= 32;
    }
    
    return nSlots;
}

#undef T
