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
@property (nonatomic, assign) BOOL stopRunloop;
@property (nonatomic, assign) off_t contentLength;
@property (nonatomic, assign) off_t bytesCanRead;
@property (nonatomic, assign) off_t bytesOffset;

- (void)doDecoderFile:(NSString*)url;
- (int)wait;
- (void)signal;

- (void)play;
- (void)stop;
- (void)seek:(NSTimeInterval)seekToTime;

- (void)selectIpodEQPreset:(NSInteger)index;
- (void)changeEQ:(int)index value:(CGFloat)v;

@end

@protocol AQDecoderDelegate <NSObject>

- (void)AQDecoder:(AQDecoder*)decoder duration:(NSTimeInterval)duration zeroCurrentTime:(BOOL)flag;
- (void)AQDecoder:(AQDecoder*)decoder timerStop:(BOOL)flag;
- (void)AQDecoder:(AQDecoder*)decoder playNext:(BOOL)flag;

@end
