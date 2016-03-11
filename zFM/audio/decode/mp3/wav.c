//
//  wav.c
//  mp3
//
//  Created by zykhbl on 16/3/8.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include "common.h"

#include "wav.h"

#define T wav

struct T {
    char    riff[4];
    int     riff_size;
    char    wave[4];
    char    fmt[4];
    int     fmt_size;
    short   format;
    short   channels;
    int     frequency;
    int     bitRate;
    short   sampling_frequency;
    short   bits;
    char    data[4];
    int     data_size;
};

void writeString(FILE *f, char *buf) {
    fwrite(buf, strlen(buf), 1, f);
}

void writeInt(FILE *f, int i) {
    fwrite(&i, sizeof(i), 1, f);
}

void writeShort(FILE *f, short i) {
    fwrite(&i, sizeof(i), 1, f);
}

void writeWAVHeader(FILE *f) {
    int len = 80 * 1024 * 1024;
    short channels = 2;
    
    //RIFF header
    writeString(f, "RIFF");//riff id
    writeInt(f, len - 8);//riff chunk size *PLACEHOLDER*
    writeString(f, "WAVE");//wave type
    
    //fmt chunk
    writeString(f, "fmt ");//fmt id
    writeInt(f, 16);//fmt chunk size
    writeShort(f, 1);//format: 1(PCM)
    writeShort(f, channels);//channels
    writeInt(f, 44100);//samples per second
    writeInt(f, (int) (channels * 44100 * (16 / 8)));//BPSecond
    writeShort(f, (short) (channels * (16 / 8)));//BPSample
    writeShort(f, (short) (16));//bPSample
    
    //data chunk
    writeString(f, "data");//data id
    writeInt(f, len - 44);//data chunk size *PLACEHOLDER*
}

void out_fifo(short pcm_sample[2][SSLIMIT][SBLIMIT], int num, int stereo, int done, FILE *outFile, unsigned long *psampFrames) {
    int i, j, l;
    static short int outsamp[1600];
    static long k = 0;
    
    if (!done) {
        for (i = 0; i < num; i++) {
            for (j = 0; j < SBLIMIT; j++) {
                (*psampFrames)++;
                for (l = 0; l < stereo; l++) {
                    if (!(k % 1600) && k) {//k > 0 且 k 整除 1600 时写入文件
                        fwrite(outsamp, 2, 1600, outFile);
                        k = 0;
                    }
                    outsamp[k++] = pcm_sample[l][i][j];
                }
            }
        }
    } else {
        fwrite(outsamp, 2, (int)k, outFile);
        k = 0;
    }
}

#undef T
