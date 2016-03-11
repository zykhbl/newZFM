//
//  AQDecoder.h
//  zFM
//
//  Created by zykhbl on 16/3/11.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#import <UIKit/UIKit.h>
#include <pthread.h>
#include "bit_stream.h"

@protocol AQDecoderDelegate;

@interface AQDecoder : NSObject

@property (nonatomic, weak) id<AQDecoderDelegate> delegate;

- (void)doDecoderFile:(NSString*)url;
- (void)signal;

- (void)play;
- (void)stop;
- (void)seek:(NSTimeInterval)seekToTime;

- (void)setContentLength:(off_t)len;
- (void)setBytesCanRead:(off_t)bytes;
- (void)setStopRunloop:(BOOL)stop;

- (void)selectIpodEQPreset:(NSInteger)index;
- (void)changeEQ:(int)index value:(CGFloat)v;

@end

@protocol AQDecoderDelegate <NSObject>

- (void)AQDecoder:(AQDecoder*)decoder duration:(NSTimeInterval)duration zeroCurrentTime:(BOOL)flag;
- (void)AQDecoder:(AQDecoder*)decoder timerStop:(BOOL)flag;
- (void)AQDecoder:(AQDecoder*)decoder playNext:(BOOL)flag;

@end
