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
#import "common.h"
#import "wav.h"
#import "decode.h"

#define kDefaultSize 1024 * 10

static pthread_mutex_t mutex;
static pthread_cond_t cond;

static id<AQDecoderDelegate> afioDelegate;
static off_t contentLength;
static off_t bytesCanRead;
static off_t bytesOffset;
static BOOL stopRunloop;

typedef struct {
    AudioFileID                  srcFileID;
    SInt64                       srcFilePos;
    char                         *srcBuffer;
    UInt32                       srcBufferSize;
    CAStreamBasicDescription     srcFormat;
    UInt32                       srcSizePerPacket;
    AudioStreamPacketDescription *packetDescriptions;
    
    UInt64                       audioDataOffset;
    UInt32                       bitRate;
    NSTimeInterval               duration;
} AudioFileIO, *AudioFileIOPtr;

static void timerStop(BOOL flag) {
    if (afioDelegate && [afioDelegate respondsToSelector:@selector(AQDecoder:timerStop:)]) {
        [afioDelegate AQDecoder:nil timerStop:flag];
    }
}

static OSStatus encoderDataProc(AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData) {
    AudioFileIOPtr afio = (AudioFileIOPtr)inUserData;
    
    UInt32 maxPackets = afio->srcBufferSize / afio->srcSizePerPacket;
    if (*ioNumberDataPackets > maxPackets) {
        *ioNumberDataPackets = maxPackets;
    }
    
    pthread_mutex_lock(&mutex);
    while ((bytesOffset + afio->srcBufferSize) > bytesCanRead && !stopRunloop) {
        if (bytesCanRead < contentLength) {
            timerStop(YES);
            pthread_cond_wait(&cond, &mutex);
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    
    UInt32 outNumBytes;
    OSStatus error = AudioFileReadPackets(afio->srcFileID, false, &outNumBytes, afio->packetDescriptions, afio->srcFilePos, ioNumberDataPackets, afio->srcBuffer);
    if (error) { NSLog(@"Input Proc Read error: %d (%4.4s)\n", (int)error, (char*)&error); return error; }
    
    bytesOffset += outNumBytes;
    afio->srcFilePos += *ioNumberDataPackets;
    
    ioData->mBuffers[0].mData = afio->srcBuffer;
    ioData->mBuffers[0].mDataByteSize = outNumBytes;
    ioData->mBuffers[0].mNumberChannels = afio->srcFormat.mChannelsPerFrame;
    
    if (outDataPacketDescription) {
        if (afio->packetDescriptions) {
            *outDataPacketDescription = afio->packetDescriptions;
        } else {
            *outDataPacketDescription = NULL;
        }
    }
    
    return error;
}

static void readCookie(AudioFileID sourceFileID, AudioConverterRef converter) {
    UInt32 cookieSize = 0;
    OSStatus error = AudioFileGetPropertyInfo(sourceFileID, kAudioFilePropertyMagicCookieData, &cookieSize, NULL);
    
    if (noErr == error && 0 != cookieSize) {
        char *cookie = new char [cookieSize];
        
        error = AudioFileGetProperty(sourceFileID, kAudioFilePropertyMagicCookieData, &cookieSize, cookie);
        if (noErr == error) {
            error = AudioConverterSetProperty(converter, kAudioConverterDecompressionMagicCookie, cookieSize, cookie);
            if (error) { NSLog(@"Could not Set kAudioConverterDecompressionMagicCookie on the Audio Converter!\n"); }
        } else {
            NSLog(@"Could not Get kAudioFilePropertyMagicCookieData from source file!\n");
        }
        
        delete [] cookie;
    }
}

@interface AQDecoder ()

@property (nonatomic, assign) CFURLRef sourceURL;
@property (nonatomic, assign) AudioConverterRef converter;
@property (nonatomic, assign) AudioFileID sourceFileID;
@property (nonatomic, assign) AudioFileIOPtr afio;
@property (nonatomic, assign) char *outputBuffer;
@property (nonatomic, assign) AudioStreamPacketDescription *outputPacketDescriptions;
@property (nonatomic, strong) AQGraph *graph;
@property (nonatomic, assign) BOOL again;

@end

@implementation AQDecoder

@synthesize delegate;

@synthesize sourceURL;
@synthesize converter;
@synthesize sourceFileID;
@synthesize afio;
@synthesize outputBuffer;
@synthesize graph;
@synthesize again;

- (void)clear {
    if (self.sourceURL) {
        CFRelease(self.sourceURL);
        self.sourceURL = NULL;
    }
    
    if (self.converter) {
        AudioConverterDispose(self.converter);
    }
    
    if (self.sourceFileID) {
        AudioFileClose(self.sourceFileID);
    }
    
    if (self.afio != NULL) {
        if (self.afio->srcBuffer) {
            delete [] self.afio->srcBuffer;
        }
        
        if (self.afio->packetDescriptions) {
            delete [] self.afio->packetDescriptions;
        }
        
        free(self.afio);
    }
    
    if (self.outputBuffer) {
        delete [] self.outputBuffer;
    }
    
    if (self.outputPacketDescriptions) {
        delete [] self.outputPacketDescriptions;
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
        
        afioDelegate = nil;
        contentLength = bytesCanRead = bytesOffset = 0;
        stopRunloop = NO;
        
        self.sourceURL = NULL;
        self.sourceFileID = 0;
        self.converter = 0;
        self.outputBuffer = NULL;
        self.outputPacketDescriptions = NULL;
        self.again = NO;
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

- (void)doConvertFile:(NSString*)url srcFilePos:(SInt64)pos {
    CAStreamBasicDescription srcFormat, dstFormat;
    
    self.afio = (AudioFileIOPtr)malloc(sizeof(AudioFileIO));
    bzero(self.afio, sizeof(AudioFileIO));
    afioDelegate = self.delegate;
    
    try {
        if (self.sourceURL == NULL) {
            self.sourceURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)url, kCFURLPOSIXPathStyle, false);
        }
        
        pthread_mutex_lock(&mutex);
        OSStatus error = AudioFileOpenURL(sourceURL, kAudioFileReadPermission, 0, &sourceFileID);
        while (error && !stopRunloop) {
            if (bytesCanRead > contentLength * 0.01) {
                pthread_mutex_unlock(&mutex);
                if (self.delegate && [self.delegate respondsToSelector:@selector(AQDecoder:playNext:)]) {
                    [self.delegate AQDecoder:self playNext:YES];
                }
                return;
            }
            
            timerStop(YES);
            pthread_cond_wait(&cond, &mutex);
            error = AudioFileOpenURL(sourceURL, kAudioFileReadPermission, 0, &sourceFileID);
        }
        pthread_mutex_unlock(&mutex);
        
        UInt32 size = sizeof(srcFormat);
        XThrowIfError(AudioFileGetProperty(self.sourceFileID, kAudioFilePropertyDataFormat, &size, &srcFormat), "couldn't get source data format");
        
        size = sizeof(self.afio->audioDataOffset);
        XThrowIfError(AudioFileGetProperty(self.sourceFileID, kAudioFilePropertyDataOffset, &size, &self.afio->audioDataOffset), "couldn't get kAudioFilePropertyDataOffset");
        if (bytesOffset == 0) {
            bytesOffset = self.afio->audioDataOffset;
        }
        
        size = sizeof(self.afio->bitRate);
        XThrowIfError(AudioFileGetProperty(self.sourceFileID, kAudioFilePropertyBitRate, &size, &self.afio->bitRate), "couldn't get kAudioFilePropertyBitRate");
        if (self.delegate && [self.delegate respondsToSelector:@selector(AQDecoder:duration:zeroCurrentTime:)]) {
            self.afio->duration = (contentLength - self.afio->audioDataOffset) * 8 / self.afio->bitRate;
            [self.delegate AQDecoder:self duration:self.afio->duration zeroCurrentTime:(pos == 0 ? YES : NO)];
        }
        
        dstFormat.mSampleRate = srcFormat.mSampleRate;
        dstFormat.mFormatID = kAudioFormatLinearPCM;
        dstFormat.mChannelsPerFrame = srcFormat.NumberChannels();
        dstFormat.mBitsPerChannel = 16;
        dstFormat.mBytesPerPacket = dstFormat.mBytesPerFrame = 2 * dstFormat.mChannelsPerFrame;
        dstFormat.mFramesPerPacket = 1;
        dstFormat.mFormatFlags = kLinearPCMFormatFlagIsPacked | kLinearPCMFormatFlagIsSignedInteger;
        
        XThrowIfError(AudioConverterNew(&srcFormat, &dstFormat, &converter), "AudioConverterNew failed!");
        
        readCookie(self.sourceFileID, self.converter);
        
        size = sizeof(srcFormat);
        XThrowIfError(AudioConverterGetProperty(self.converter, kAudioConverterCurrentInputStreamDescription, &size, &srcFormat), "AudioConverterGetProperty kAudioConverterCurrentInputStreamDescription failed!");
        
        size = sizeof(dstFormat);
        XThrowIfError(AudioConverterGetProperty(self.converter, kAudioConverterCurrentOutputStreamDescription, &size, &dstFormat), "AudioConverterGetProperty kAudioConverterCurrentOutputStreamDescription failed!");
        
        [self createGraph:dstFormat];
        
        self.afio->srcFileID = self.sourceFileID;
        self.afio->srcBufferSize = kDefaultSize;
        self.afio->srcBuffer = new char [self.afio->srcBufferSize];
        self.afio->srcFilePos = pos;
        self.afio->srcFormat = srcFormat;
        
        if (srcFormat.mBytesPerPacket == 0) {
            size = sizeof(self.afio->srcSizePerPacket);
            XThrowIfError(AudioFileGetProperty(self.sourceFileID, kAudioFilePropertyMaximumPacketSize, &size, &self.afio->srcSizePerPacket), "AudioFileGetProperty kAudioFilePropertyMaximumPacketSize failed!");
            self.afio->packetDescriptions = new AudioStreamPacketDescription[self.afio->srcBufferSize / self.afio->srcSizePerPacket];
        } else {
            self.afio->srcSizePerPacket = srcFormat.mBytesPerPacket;
            self.afio->packetDescriptions = NULL;
        }
        
        UInt32 outputSizePerPacket = dstFormat.mBytesPerPacket;
        UInt32 theOutputBufSize = kDefaultSize;
        self.outputBuffer = new char[theOutputBufSize];
        
        if (outputSizePerPacket == 0) {
            size = sizeof(outputSizePerPacket);
            XThrowIfError(AudioConverterGetProperty(self.converter, kAudioConverterPropertyMaximumOutputPacketSize, &size, &outputSizePerPacket), "AudioConverterGetProperty kAudioConverterPropertyMaximumOutputPacketSize failed!");
            self.outputPacketDescriptions = new AudioStreamPacketDescription [theOutputBufSize / outputSizePerPacket];
        }
        
        UInt32 numOutputPackets = theOutputBufSize / outputSizePerPacket;
        while (!stopRunloop) {
            AudioBufferList fillBufList;
            fillBufList.mNumberBuffers = 1;
            fillBufList.mBuffers[0].mNumberChannels = dstFormat.mChannelsPerFrame;
            fillBufList.mBuffers[0].mDataByteSize = theOutputBufSize;
            fillBufList.mBuffers[0].mData = self.outputBuffer;
            
            UInt32 ioOutputDataPackets = numOutputPackets;
            error = AudioConverterFillComplexBuffer(self.converter, encoderDataProc, self.afio, &ioOutputDataPackets, &fillBufList, self.outputPacketDescriptions);
            if (error) {
                if (kAudioConverterErr_HardwareInUse == error) {
                    NSLog(@"Audio Converter returned kAudioConverterErr_HardwareInUse!\n");
                } else {
                    XThrowIfError(error, "AudioConverterFillComplexBuffer error!");
                }
            } else {
                if (ioOutputDataPackets == 0) {
                    self.again = YES;
                    break;
                }
            }
            
            if (noErr == error) {
                timerStop(NO);
                
                [self.graph addBuf:fillBufList.mBuffers[0].mData numberBytes:fillBufList.mBuffers[0].mDataByteSize];
            }
        }
    } catch (CAXException e) {
        char buf[256];
        NSLog(@"Error: %s (%s)\n", e.mOperation, e.FormatError(buf));
    }
    
    if (self.again) {
        self.again = NO;
        SInt64 pos = self.afio->srcFilePos;
        [self clear];
        [self doConvertFile:url srcFilePos:pos];
    }
}

- (void)doDecoderFile:(NSString*)url srcFilePos:(SInt64)pos {
    char *mp3_filename = (char *)[url cStringUsingEncoding:NSUTF8StringEncoding];
    struct bit_stream *bs = create_bit_stream(mp3_filename, BUFFER_SIZE);
//    struct audio_data_buf *buf = create_audio_data_buf(BUFFER_SIZE);
//    struct frame *fr_ps = create_frame();
//    struct III_side_info *side_info = create_III_side_info();
//    III_scalefac_t III_scalefac;
//    
//    unsigned long frameNum = 0;
//    unsigned int old_crc;
//    int done = FALSE;
//    
//    typedef short PCM[2][SSLIMIT][SBLIMIT];
//    PCM *pcm_sample;
//    
//    pcm_sample = (PCM *)mem_alloc((long) sizeof(PCM), "PCM Samp");
//    
//    unsigned long sample_frames = 0;
//    while(!end_bs(bs)) {
//        int sync = seek_sync(bs, SYNC_WORD, SYNC_WORD_LENGTH);//尝试帧同步
//        if (!sync) {
//            done = TRUE;
//            printf("\nFrame cannot be located\n");
////            out_fifo(*pcm_sample, 3, fr_ps->stereo, done, musicout, &sample_frames);
//            break;
//        }
//        
//        decode_info(bs, fr_ps);//解码帧头
//        
//        hdr_to_frps(fr_ps);//将fr_ps.header中的信息解读到fr_ps的相关域中
//        
//        if(frameNum == 0) {//输出相关信息
//            writeHdr(fr_ps);
//        }
//        
//        printf("\r%05lu", frameNum++);
//        
//        if (fr_ps->header.error_protection) {//如果有的话，读取错误码
//            buffer_CRC(bs, &old_crc);
//        }
//        
//        switch (fr_ps->header.lay) {
//            case 3: {
//                //main_data_end 可以不初始化为0，但如果不初始化，它会在使用了栈中上一次循环遗留下的值，虽然不影响结果，但是会使打印信息看起来很怪
//                int nSlots, main_data_end = 0, flush_main;
//                int bytes_to_discard, gr, ch, ss, sb;
//                unsigned long bitsPerSlot = 8;
//                static int frame_start = 0;
//                
//                III_get_side_info(bs, side_info, fr_ps);//读取Side信息
//                
//                nSlots = main_data_slots(fr_ps);//计算slot个数
//                
//                for (; nSlots > 0; nSlots--) {//读主数据(Audio Data)
//                    hputbuf(buf, (unsigned int)getbits(bs, 8), 8);
//                }
//                
//                main_data_end = (int)(hsstell(buf) / 8);//of privious frame
//                if ((flush_main = (int)(hsstell(buf) % bitsPerSlot))) {
//                    hgetbits(buf, (int)(bitsPerSlot - flush_main));
//                    main_data_end++;
//                }
//                
//                bytes_to_discard = frame_start - main_data_end - side_info->main_data_begin;
//                unsigned buf_size = hget_buf_size(buf);
//                if(main_data_end > buf_size) {//环结构，缓存数组从[0, buf_size - 1], 不需要 >= buf_size，因为hputbuf，hgetbits等操作做了求余；不用应该也可以
//                    frame_start -= buf_size;
//                    rewindNbytes(buf, buf_size);
//                }
//                
//                frame_start += main_data_slots(fr_ps);//当前帧的结尾，同时也是下一帧的开始位置(可以直接使用变量nSlots，不用再调用函数main_data_slots计算一次)
//                
//                if (bytes_to_discard < 0) {//TODO: 实现为实时流读取时，可能要在这里控制暂停
//                    printf("Not enough main data to decode frame %ld.  Frame discarded.\n", frameNum - 1);
//                    break;
//                }
//                
//                for (; bytes_to_discard > 0; bytes_to_discard--) {//丢弃已读或无用的的字节
//                    hgetbits(buf, 8);
//                }
//                
//                int clip = 0;
//                for (gr = 0; gr < 2; gr++) {
//                    double lr[2][SBLIMIT][SSLIMIT], ro[2][SBLIMIT][SSLIMIT];
//                    
//                    for (ch = 0; ch < fr_ps->stereo; ch++) {//主解码
//                        long int is[SBLIMIT][SSLIMIT];//保存量化数据
//                        int part2_start;
//                        part2_start = (int)hsstell(buf);//totbit 的位置，即：当前帧音频主数据(main data)：scale_factor + hufman_code的开始位置
//                        
//                        III_get_scale_factors(buf, &III_scalefac, side_info, gr, ch, fr_ps);//获取比例因子
//                        
//                        III_hufman_decode(buf, is, side_info, ch, gr, part2_start, fr_ps);//huffman解码
//                        
//                        III_dequantize_sample(is, ro[ch], &III_scalefac, (struct gr_info_s *)&(side_info->ch[ch].gr[gr]), ch, fr_ps);//反量化采样
//                    }
//                    
//                    III_stereo(ro, lr, &III_scalefac, (struct gr_info_s *)&(side_info->ch[0].gr[gr]), fr_ps);//立体声处理
//                    
//                    for (ch = 0; ch < fr_ps->stereo; ch++) {
//                        double re[SBLIMIT][SSLIMIT];
//                        double hybridIn[SBLIMIT][SSLIMIT];//hybrid filter input
//                        double hybridOut[SBLIMIT][SSLIMIT];//hybrid filter out
//                        double polyPhaseIn[SBLIMIT];//polyPhase input
//                        
//                        III_reorder(lr[ch], re, (struct gr_info_s *)&(side_info->ch[ch].gr[gr]), fr_ps);
//                        
//                        III_antialias(re, hybridIn, (struct gr_info_s *)&(side_info->ch[ch].gr[gr]), fr_ps);//抗锯齿处理
//                        
//                        for (sb = 0; sb < SBLIMIT; sb++) {//IMDCT(hybrid synthesis)
//                            III_hybrid(hybridIn[sb], hybridOut[sb], sb, ch, (struct gr_info_s *)&(side_info->ch[ch].gr[gr]), fr_ps);
//                        }
//                        
//                        for (ss = 0; ss < 18; ss++) {//多相频率倒置
//                            for (sb = 0; sb < SBLIMIT; sb++) {
//                                if ((ss % 2) && (sb % 2)) {
//                                    hybridOut[sb][ss] = -hybridOut[sb][ss];
//                                }
//                            }
//                        }
//                        
//                        for (ss = 0; ss < 18; ss++) {//多相合成
//                            for (sb = 0; sb < SBLIMIT; sb++) {
//                                polyPhaseIn[sb] = hybridOut[sb][ss];
//                            }
//                            
//                            clip += subBandSynthesis(polyPhaseIn, ch, &((*pcm_sample)[ch][ss][0]));//子带合成
//                        }
//                    }
//                    
////                    out_fifo(*pcm_sample, 18, fr_ps->stereo, done, musicout, &sample_frames);//PCM输出(Output PCM sample points for one granule(颗粒))
//                }
//                
//                if(clip > 0) {
//                    printf("\n%d samples clipped.\n", clip);
//                }
//                
//                break;
//            }
//            default: {
//                printf("\nOnly layer III supported!\n");
//                exit(1);
//                break;
//            }
//        }
//    }
//    
//    free_III_side_info(&side_info);
//    free_frame(&fr_ps);
//    free_audio_data_buf(&buf);
//    free_bit_stream(&bs);
//    printf("\nDecoding done.\n");
}

- (void)doDecoderFile:(NSString*)url {
    [self doDecoderFile:url srcFilePos:0];
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
    bytesOffset = self.afio->audioDataOffset + (contentLength - self.afio->audioDataOffset) * (seekToTime / self.afio->duration);
    
    double packetDuration = self.afio->srcFormat.mFramesPerPacket / self.afio->srcFormat.mSampleRate;
    self.afio->srcFilePos = (UInt32)(seekToTime / packetDuration);
    [self signal];
}

- (void)setContentLength:(off_t)len {
    contentLength = len;
}

- (void)setBytesCanRead:(off_t)bytes {
    bytesCanRead = bytes;
}

- (void)setStopRunloop:(BOOL)stop {
    stopRunloop = stop;
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
