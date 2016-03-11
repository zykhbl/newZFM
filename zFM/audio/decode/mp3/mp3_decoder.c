//
//  mp3_decoder.c
//  mp3Decoder
//
//  Created by zykhbl on 16/2/24.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

//printf("\n1: totbit = %ld, frame_start = %d, main_data_end = %d, main_data_begin = %d, bytes_to_discard = %d \n", hsstell() / 8, frame_start, main_data_end, III_side_info.main_data_begin, bytes_to_discard);

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "wav.h"
#include "decode.h"

#define mp3_filename "/Users/weidong_wu/mp3/resource/mymp3.mp3"
#define pcm_filename "/Users/weidong_wu/mp3/resource/mypcm.wav"

int main(int argc, char**argv) {
    FILE *musicout;
    if ((musicout = fopen(pcm_filename, "w+b")) == NULL) {
        printf ("Could not create \"%s\".\n", pcm_filename);
        exit(1);
    }
    writeWAVHeader(musicout);
    
    bit_stream bs = create_bit_stream(mp3_filename, BUFFER_SIZE);
    audio_data_buf buf = create_audio_data_buf(BUFFER_SIZE);
    frame fr_ps = create_frame();
    III_side_info side_info = create_III_side_info();
    III_scalefac_t III_scalefac;
    
    unsigned long frameNum = 0;
    unsigned int old_crc;
    int done = FALSE;
    
    typedef short PCM[2][SSLIMIT][SBLIMIT];
    PCM *pcm_sample;
    
    pcm_sample = (PCM *)mem_alloc((long) sizeof(PCM), "PCM Samp");
    
    unsigned long sample_frames = 0;
    while(!end_bs(bs)) {
        int sync = seek_sync(bs, SYNC_WORD, SYNC_WORD_LENGTH);//尝试帧同步
        if (!sync) {
            done = TRUE;
            printf("\nFrame cannot be located\n");
            out_fifo(*pcm_sample, 3, fr_ps->stereo, done, musicout, &sample_frames);
            break;
        }

        decode_info(bs, fr_ps);//解码帧头

        hdr_to_frps(fr_ps);//将fr_ps.header中的信息解读到fr_ps的相关域中

        if(frameNum == 0) {//输出相关信息
            writeHdr(fr_ps);
        }
        
        printf("\r%05lu", frameNum++);
        
        if (fr_ps->header.error_protection) {//如果有的话，读取错误码
            buffer_CRC(bs, &old_crc);
        }
        
        switch (fr_ps->header.lay) {
            case 3: {
                //main_data_end 可以不初始化为0，但如果不初始化，它会在使用了栈中上一次循环遗留下的值，虽然不影响结果，但是会使打印信息看起来很怪
                int nSlots, main_data_end = 0, flush_main;
                int bytes_to_discard, gr, ch, ss, sb;
                unsigned long bitsPerSlot = 8;
                static int frame_start = 0;
                
                III_get_side_info(bs, side_info, fr_ps);//读取Side信息
                
                nSlots = main_data_slots(fr_ps);//计算slot个数
                
                for (; nSlots > 0; nSlots--) {//读主数据(Audio Data)
                    hputbuf(buf, (unsigned int)getbits(bs, 8), 8);
                }
                
                main_data_end = (int)(hsstell(buf) / 8);//of privious frame
                if ((flush_main = (int)(hsstell(buf) % bitsPerSlot))) {
                    hgetbits(buf, (int)(bitsPerSlot - flush_main));
                    main_data_end++;
                }
                
                bytes_to_discard = frame_start - main_data_end - side_info->main_data_begin;
                unsigned buf_size = hget_buf_size(buf);
                if(main_data_end > buf_size) {//环结构，缓存数组从[0, buf_size - 1], 不需要 >= buf_size，因为hputbuf，hgetbits等操作做了求余；不用应该也可以
                    frame_start -= buf_size;
                    rewindNbytes(buf, buf_size);
                }
                
                frame_start += main_data_slots(fr_ps);//当前帧的结尾，同时也是下一帧的开始位置(可以直接使用变量nSlots，不用再调用函数main_data_slots计算一次)
                
                if (bytes_to_discard < 0) {//TODO: 实现为实时流读取时，可能要在这里控制暂停
                    printf("Not enough main data to decode frame %ld.  Frame discarded.\n", frameNum - 1);
                    break;
                }
                
                for (; bytes_to_discard > 0; bytes_to_discard--) {//丢弃已读或无用的的字节
                    hgetbits(buf, 8);
                }
                
                int clip = 0;
                for (gr = 0; gr < 2; gr++) {
                    double lr[2][SBLIMIT][SSLIMIT], ro[2][SBLIMIT][SSLIMIT];

                    for (ch = 0; ch < fr_ps->stereo; ch++) {//主解码
                        long int is[SBLIMIT][SSLIMIT];//保存量化数据
                        int part2_start;
                        part2_start = (int)hsstell(buf);//totbit 的位置，即：当前帧音频主数据(main data)：scale_factor + hufman_code的开始位置

                        III_get_scale_factors(buf, &III_scalefac, side_info, gr, ch, fr_ps);//获取比例因子

                        III_hufman_decode(buf, is, side_info, ch, gr, part2_start, fr_ps);//huffman解码

                        III_dequantize_sample(is, ro[ch], &III_scalefac, &(side_info->ch[ch].gr[gr]), ch, fr_ps);//反量化采样
                    }

                    III_stereo(ro, lr, &III_scalefac, &(side_info->ch[0].gr[gr]), fr_ps);//立体声处理
                    
                    for (ch = 0; ch < fr_ps->stereo; ch++) {
                        double re[SBLIMIT][SSLIMIT];
                        double hybridIn[SBLIMIT][SSLIMIT];//hybrid filter input
                        double hybridOut[SBLIMIT][SSLIMIT];//hybrid filter out
                        double polyPhaseIn[SBLIMIT];//polyPhase input
                        
                        III_reorder(lr[ch], re, &(side_info->ch[ch].gr[gr]), fr_ps);

                        III_antialias(re, hybridIn, &(side_info->ch[ch].gr[gr]), fr_ps);//抗锯齿处理

                        for (sb = 0; sb < SBLIMIT; sb++) {//IMDCT(hybrid synthesis)
                            III_hybrid(hybridIn[sb], hybridOut[sb], sb, ch, &(side_info->ch[ch].gr[gr]), fr_ps);
                        }
                        
                        for (ss = 0; ss < 18; ss++) {//多相频率倒置
                            for (sb = 0; sb < SBLIMIT; sb++) {
                                if ((ss % 2) && (sb % 2)) {
                                    hybridOut[sb][ss] = -hybridOut[sb][ss];
                                }
                            }
                        }
                        
                        for (ss = 0; ss < 18; ss++) {//多相合成
                            for (sb = 0; sb < SBLIMIT; sb++) {
                                polyPhaseIn[sb] = hybridOut[sb][ss];
                            }

                            clip += subBandSynthesis(polyPhaseIn, ch, &((*pcm_sample)[ch][ss][0]));//子带合成
                        }
                    }

                    out_fifo(*pcm_sample, 18, fr_ps->stereo, done, musicout, &sample_frames);//PCM输出(Output PCM sample points for one granule(颗粒))
                }
                
                if(clip > 0) {
                    printf("\n%d samples clipped.\n", clip);
                }
                
                break;
            }
            default: {
                printf("\nOnly layer III supported!\n");   
                exit(1);   
                break;
            }
        }
    }
    
    free_III_side_info(&side_info);
    free_frame(&fr_ps);
    free_audio_data_buf(&buf);
    free_bit_stream(&bs);
    fclose(musicout);   
    printf("\nDecoding done.\n");
    
    return 0;
}

