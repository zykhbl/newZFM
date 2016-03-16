//
//  AQDecoder.h
//  zFM
//
//  Created by zykhbl on 16/3/11.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#import "AQDecoder.h"
#import <AudioToolbox/AudioToolbox.h>
#import "CAXException.h"
#import "CAStreamBasicDescription.h"
#import "AQGraph.h"
#import "IpodEQ.h"
#import "CustomEQ.h"
#import <stdlib.h>
#import <stdio.h>
#import <string.h>
#include "common.h"
#include "wav.h"
#include "decode.h"

#define kDefaultSize 1024 * 10

@interface AQDecoder ()

@property (nonatomic, assign) CFURLRef sourceURL;
@property (nonatomic, assign) AudioFileID sourceFileID;
@property (nonatomic, assign) CAStreamBasicDescription *srcFormat;

@property (nonatomic, strong) AQGraph *graph;

@property (nonatomic, assign) pthread_mutex_t mutex;
@property (nonatomic, assign) pthread_cond_t cond;
@property (nonatomic, assign) BOOL isSeek;
@property (nonatomic, assign) SInt64 srcFilePos;
@property (nonatomic, assign) UInt64 audioDataOffset;
@property (nonatomic, assign) UInt32 bitRate;
@property (nonatomic, assign) NSTimeInterval duration;

@end

@implementation AQDecoder

@synthesize delegate;
@synthesize stopRunloop;
@synthesize contentLength;
@synthesize bytesCanRead;
@synthesize bytesOffset;

@synthesize sourceURL;
@synthesize sourceFileID;
@synthesize srcFormat;

@synthesize graph;

@synthesize mutex;
@synthesize cond;
@synthesize isSeek;
@synthesize srcFilePos;
@synthesize audioDataOffset;
@synthesize bitRate;
@synthesize duration;

- (void)clear {
    if (self.sourceURL) {
        CFRelease(self.sourceURL);
        self.sourceURL = NULL;
    }
    
    if (self.sourceFileID) {
        AudioFileClose(self.sourceFileID);
    }
    
    if (self.srcFormat) {
        free(self.srcFormat);
        self.srcFormat = NULL;
    }
}

- (void)dealloc {
    [self clear];
    
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

- (id)init {
    self = [super init];
    
    if (self) {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);

        self.contentLength = self.bytesCanRead = self.bytesOffset = 0;
        self.stopRunloop = NO;
        
        self.sourceURL = NULL;
        self.sourceFileID = 0;
    }
    
    return self;
}

- (void)createGraph:(CAStreamBasicDescription)dstFormat {
    if (self.graph == nil) {
        self.graph = [[AQGraph alloc] init];
        
        [self.graph awakeFromNib];
        [self.graph setasbd:dstFormat];
        [self.graph initializeAUGraph];
        
        [self.graph enableInput:0 isOn:1.0];
        [self.graph enableInput:1 isOn:0.0];
        [self.graph setInputVolume:0 value:1.0];
        [self.graph setInputVolume:1 value:0.0];
        [self.graph setOutputVolume:1.0];
        
        [self.graph startAUGraph];
        
        [self.graph selectIpodEQPreset:[[IpodEQ sharedIpodEQ] selected]];
        for (int i = 0; i < [[[CustomEQ sharedCustomEQ] eqFrequencies] count]; ++i) {
            CGFloat eqValue = [[CustomEQ sharedCustomEQ] getEQValueInIndex:i];
            [self.graph changeEQ:i value:eqValue];
        }
    }
}

- (void)timerStop:(BOOL)flag {
    if (self.delegate && [self.delegate respondsToSelector:@selector(AQDecoder:timerStop:)]) {
        [self.delegate AQDecoder:nil timerStop:flag];
    }
}

- (void)doDecoderFile:(NSString*)url srcFilePos:(SInt64)pos {
    self.isSeek = NO;
    self.srcFilePos = pos;
    self.srcFormat = (CAStreamBasicDescription*)malloc(sizeof(CAStreamBasicDescription));
    CAStreamBasicDescription dstFormat;
    
    try {
        if (self.sourceURL == NULL) {
            self.sourceURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)url, kCFURLPOSIXPathStyle, false);
        }
        
        pthread_mutex_lock(&mutex);
        OSStatus error = AudioFileOpenURL(sourceURL, kAudioFileReadPermission, 0, &sourceFileID);
        while (error && !self.stopRunloop) {
            if (self.bytesCanRead > self.contentLength * 0.01) {
                pthread_mutex_unlock(&mutex);
                if (self.delegate && [self.delegate respondsToSelector:@selector(AQDecoder:playNext:)]) {
                    [self.delegate AQDecoder:self playNext:YES];
                }
                return;
            }
            
            [self timerStop:YES];
            pthread_cond_wait(&cond, &mutex);
            error = AudioFileOpenURL(sourceURL, kAudioFileReadPermission, 0, &sourceFileID);
        }
        pthread_mutex_unlock(&mutex);
        
        UInt32 size = sizeof(*srcFormat);
        XThrowIfError(AudioFileGetProperty(self.sourceFileID, kAudioFilePropertyDataFormat, &size, self.srcFormat), "couldn't get source data format");
        
        size = sizeof(self.audioDataOffset);
        XThrowIfError(AudioFileGetProperty(self.sourceFileID, kAudioFilePropertyDataOffset, &size, &audioDataOffset), "couldn't get kAudioFilePropertyDataOffset");
        if (self.bytesOffset == 0) {
            self.bytesOffset = self.audioDataOffset;
        }
        
        size = sizeof(self.bitRate);
        XThrowIfError(AudioFileGetProperty(self.sourceFileID, kAudioFilePropertyBitRate, &size, &bitRate), "couldn't get kAudioFilePropertyBitRate");
        if (self.delegate && [self.delegate respondsToSelector:@selector(AQDecoder:duration:zeroCurrentTime:)]) {
            self.duration = (self.contentLength - self.bytesOffset) * 8 / self.bitRate;
            [self.delegate AQDecoder:self duration:self.duration zeroCurrentTime:(pos == 0 ? YES : NO)];
        }
        
        dstFormat.mSampleRate = self.srcFormat->mSampleRate;
        dstFormat.mFormatID = kAudioFormatLinearPCM;
        dstFormat.mChannelsPerFrame = self.srcFormat->NumberChannels();
        dstFormat.mBitsPerChannel = 16;
        dstFormat.mBytesPerPacket = dstFormat.mBytesPerFrame = 2 * dstFormat.mChannelsPerFrame;
        dstFormat.mFramesPerPacket = 1;
        dstFormat.mFormatFlags = kLinearPCMFormatFlagIsPacked | kLinearPCMFormatFlagIsSignedInteger;
        
        [self createGraph:dstFormat];
        
        [self decoderFrame:url];
    } catch (CAXException e) {
        char buf[256];
        NSLog(@"Error: %s (%s)\n", e.mOperation, e.FormatError(buf));
    }
}

- (void)decoderFrame:(NSString*)url {
    char *mp3_filename = (char *)[url cStringUsingEncoding:NSUTF8StringEncoding];
    struct bit_stream *bs = create_bit_stream(mp3_filename, BUFFER_SIZE, (__bridge void*)self);
    struct audio_data_buf *buf = create_audio_data_buf(BUFFER_SIZE);
    struct frame *fr_ps = create_frame();
    struct III_side_info *side_info = create_III_side_info();
    III_scalefac_t III_scalefac;
    
    unsigned long frameNum = 0;
    unsigned int old_crc;
    int done = FALSE;
    
    typedef short PCM[2][SSLIMIT][SBLIMIT];
    PCM *pcm_sample;
    
    pcm_sample = (PCM *)mem_alloc((long) sizeof(PCM), (char *)"PCM Samp");
    
    unsigned long sample_frames = 0;
    int frame_start = 0;
    short int outsamp[1600];
    long k = 0;
    
    while(!self.stopRunloop) {
        [self wait];
        
        if (self.isSeek) {
            self.isSeek = NO;
            frameNum = self.srcFilePos;
            seek_bit_stream(bs, self.bytesOffset);
            init_bit_stream(bs);
            init_audio_data_buf(buf);
            memset(fr_ps, 0, sizeof(*fr_ps));
            memset(side_info, 0, sizeof(*side_info));
            memset(&III_scalefac, 0, sizeof(III_scalefac_t));
            memset(pcm_sample, 0, sizeof(PCM));
            frame_start = 0;
            memset(outsamp, 0, sizeof(outsamp));
            k = 0;
        }
        
        int sync = seek_sync(bs, SYNC_WORD, SYNC_WORD_LENGTH);//尝试帧同步
        if (!sync) {
            done = TRUE;
            printf("\nFrame cannot be located\n");
            out_fifo(*pcm_sample, 3, fr_ps->stereo, done, outsamp, &k, &sample_frames, (__bridge void*)self);
            break;
        }
        
        decode_info(bs, fr_ps);//解码帧头
        
        hdr_to_frps(fr_ps);//将fr_ps.header中的信息解读到fr_ps的相关域中
        
        if(frameNum == 0) {//输出相关信息
            writeHdr(fr_ps);
        }
        
//        printf("%05lu\n", frameNum++);
        frameNum++;
        
        if (fr_ps->header.error_protection) {//如果有的话，读取错误码
            buffer_CRC(bs, &old_crc);
        }
        
        switch (fr_ps->header.lay) {
            case 3: {
                //main_data_end 可以不初始化为0，但如果不初始化，它会在使用了栈中上一次循环遗留下的值，虽然不影响结果，但是会使打印信息看起来很怪
                int nSlots, main_data_end = 0, flush_main;
                int bytes_to_discard, gr, ch, ss, sb;
                unsigned long bitsPerSlot = 8;
                
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
                
                if (bytes_to_discard < 0) {
                    printf("Not enough main data to decode frame %ld, Frame discarded.\n", frameNum - 1);
                    
                    break;
                }
                
                for (; bytes_to_discard > 0; bytes_to_discard--) {//丢弃已读或无用的的字节
                    hgetbits(buf, 8);
                }
                
                for (gr = 0; gr < 2; gr++) {
                    for (ch = 0; ch < fr_ps->stereo; ch++) {
                        if (side_info->ch[ch].gr[gr].window_switching_flag && side_info->ch[ch].gr[gr].block_type == 0) {
                            break;
                        }
                    }
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
                        
                        III_dequantize_sample(is, ro[ch], &III_scalefac, (struct gr_info_s *)&(side_info->ch[ch].gr[gr]), ch, fr_ps);//反量化采样
                    }
                    
                    III_stereo(ro, lr, &III_scalefac, (struct gr_info_s *)&(side_info->ch[0].gr[gr]), fr_ps);//立体声处理
                    
                    for (ch = 0; ch < fr_ps->stereo; ch++) {
                        double re[SBLIMIT][SSLIMIT];
                        double hybridIn[SBLIMIT][SSLIMIT];//hybrid filter input
                        double hybridOut[SBLIMIT][SSLIMIT];//hybrid filter out
                        double polyPhaseIn[SBLIMIT];//polyPhase input
                        
                        III_reorder(lr[ch], re, (struct gr_info_s *)&(side_info->ch[ch].gr[gr]), fr_ps);
                        
                        III_antialias(re, hybridIn, (struct gr_info_s *)&(side_info->ch[ch].gr[gr]), fr_ps);//抗锯齿处理
                        
                        for (sb = 0; sb < SBLIMIT; sb++) {//IMDCT(hybrid synthesis)
                            III_hybrid(hybridIn[sb], hybridOut[sb], sb, ch, (struct gr_info_s *)&(side_info->ch[ch].gr[gr]), fr_ps);
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
                    
                    out_fifo(*pcm_sample, 18, fr_ps->stereo, done, outsamp, &k, &sample_frames, (__bridge void*)self);//PCM输出(Output PCM sample points for one granule(颗粒))
                }
                
                if(clip > 0) {
//                    printf("\n%d samples clipped.\n", clip);
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
    printf("\nDecoding done.\n");
}

void out_fifo(short pcm_sample[2][SSLIMIT][SBLIMIT], int num, int stereo, int done, short int *outsamp, long *k, unsigned long *psampFrames, void *myself) {
    int i, j, l;
    
    if (!done) {
        for (i = 0; i < num; i++) {
            for (j = 0; j < SBLIMIT; j++) {
                (*psampFrames)++;
                for (l = 0; l < stereo; l++) {
                    if (!(*k % 1600) && *k) {//k > 0 且 k 整除 1600 时写入文件
                        AQDecoder *decoder = (__bridge AQDecoder*)myself;
                        [decoder timerStop:NO];
                        [decoder.graph addBuf:outsamp numberBytes:1600 * sizeof(short int)];
                        *k = 0;
                    }
                    outsamp[(*k)++] = pcm_sample[l][i][j];
                }
            }
        }
    } else {
        AQDecoder *decoder = (__bridge AQDecoder*)myself;
        [decoder timerStop:NO];
        [decoder.graph addBuf:outsamp numberBytes:((int)(*k)) * sizeof(short int)];
        *k = 0;
    }
}

- (void)doDecoderFile:(NSString*)url {
    [self doDecoderFile:url srcFilePos:0];
}

- (int)wait {
    pthread_mutex_lock(&mutex);
    while (self.bytesOffset + BUFFER_SIZE > self.bytesCanRead) {
        if (self.bytesCanRead < self.contentLength) {
            [self timerStop:YES];
            pthread_cond_wait(&cond, &mutex);
        } else {
            pthread_mutex_unlock(&mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

- (void)signal {
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

- (void)play {
    [self.graph startAUGraph];
}

- (void)stop {
    [self.graph stopAUGraph];
}

- (void)seek:(NSTimeInterval)seekToTime {
    self.isSeek = YES;
    
    self.bytesOffset = self.audioDataOffset + (self.contentLength - self.audioDataOffset) * (seekToTime / self.duration);
    double packetDuration = self.srcFormat->mFramesPerPacket / self.srcFormat->mSampleRate;
    self.srcFilePos = (UInt32)(seekToTime / packetDuration);
    
    [self signal];
}

- (void)selectIpodEQPreset:(NSInteger)index {
    if (self.graph != nil) {
        [self.graph selectIpodEQPreset:index];
    }
}

- (void)changeEQ:(int)index value:(CGFloat)v {
    if (self.graph != nil) {
        [self.graph changeEQ:index value:v];
    }
}

@end
