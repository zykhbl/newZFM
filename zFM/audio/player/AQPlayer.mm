//
//  AQPlayer.h
//  zFM
//
//  Created by zykhbl on 15-9-25.
//  Copyright (c) 2015年 zykhbl. All rights reserved.
//

#import "AQPlayer.h"
#import <AVFoundation/AVFoundation.h>

@implementation AQPlayer

@synthesize delegate;
@synthesize downloader;
@synthesize decoder;

+ (void)playForeground {
    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:AVAudioSessionCategoryOptionDefaultToSpeaker error:nil];
}

+ (id)sharedAQPlayer {
    static dispatch_once_t pred = 0;
    __strong static id _sharedObject = nil;
    dispatch_once(&pred, ^{
        _sharedObject = [[self alloc]init];
    });
    return _sharedObject;
}

- (void)clear {
    if (self.downloader != nil) {
        [self.downloader.conn cancel];
        [self.downloader cancel];
        self.downloader = nil;
    }
    
    if (self.decoder != nil) {
        [self.decoder setStopRunloop:YES];
        [self.decoder signal];
        self.decoder = nil;
    }
}

- (void)play:(NSString*)url {
    if (self.downloader == nil) {
        self.downloader = [[AQDownloader alloc] init];
        self.downloader.delegate = self;
        self.downloader.url = url;
        [self.downloader performSelector:@selector(start) withObject:nil afterDelay:1.0];
    }
}

- (void)play {
    [self.decoder play];
    [self.decoder signal];
}

- (void)stop {
    [self.decoder stop];
}

- (void)seek:(NSTimeInterval)seekToTime {
    [self.decoder setBytesCanRead:self.downloader.bytesReceived];
    [self.decoder seek:seekToTime];
}

- (void)selectIpodEQPreset:(NSInteger)index {
    if (self.decoder != nil) {
        [self.decoder selectIpodEQPreset:index];
    }    
}

- (void)changeEQ:(int)index value:(CGFloat)v {
    if (self.decoder != nil) {
        [self.decoder changeEQ:index value:v];
    }
}

//===========AQDownloaderDelegate===========
- (void)AQDownloader:(AQDownloader*)downloader convert:(NSString*)filePath {
    __weak typeof(self) weak_self = self;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        if (weak_self.decoder == nil) {
            weak_self.decoder = [[AQDecoder alloc] init];
            weak_self.decoder.delegate = weak_self;
            [weak_self.decoder setContentLength:weak_self.downloader.contentLength];
            [weak_self.decoder setBytesCanRead:weak_self.downloader.bytesReceived];
        }
        [weak_self.decoder doDecoderFile:filePath];
    });
}

- (void)AQDownloader:(AQDownloader*)downloader signal:(BOOL)flag {
    if (self.decoder != nil) {
        [self.decoder setBytesCanRead:self.downloader.bytesReceived];
        [self.decoder signal];
    }
}

- (void)timerStop:(BOOL)flag {
    if (self.delegate && [self.delegate respondsToSelector:@selector(AQPlayer:timerStop:)]) {
        [self.delegate AQPlayer:self timerStop:flag];
    }
}

- (void)AQDownloader:(AQDownloader*)downloader fail:(BOOL)flag {
    if (self.delegate && [self.delegate respondsToSelector:@selector(AQPlayer:duration:zeroCurrentTime:)]) {
        [self.delegate AQPlayer:self duration:0.0 zeroCurrentTime:flag];
    }
    [self timerStop:flag];
}

- (void)AQDownloader:(AQDownloader*)downloader playNext:(BOOL)flag {
    if (self.delegate && [self.delegate respondsToSelector:@selector(AQPlayer:playNext:)]) {
        [self.delegate AQPlayer:self playNext:flag];
    }
}

//===========AQConverterDelegate===========
- (void)AQDecoder:(AQDecoder*)decoder duration:(NSTimeInterval)duration zeroCurrentTime:(BOOL)flag {
    if (self.delegate && [self.delegate respondsToSelector:@selector(AQPlayer:duration:zeroCurrentTime:)]) {
        [self.delegate AQPlayer:self duration:duration zeroCurrentTime:flag];
    }
}

- (void)AQDecoder:(AQDecoder*)decoder timerStop:(BOOL)flag {
    [self timerStop:flag];
}

- (void)AQDecoder:(AQDecoder*)decoder playNext:(BOOL)flag {
    if (self.delegate && [self.delegate respondsToSelector:@selector(AQPlayer:playNext:)]) {
        [self.delegate AQPlayer:self playNext:flag];
    }
}

@end
