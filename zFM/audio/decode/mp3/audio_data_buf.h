//
//  audio_data_buf.h
//  mp3
//
//  Created by zykhbl on 16/3/10.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef audio_data_buf_h
#define audio_data_buf_h

#define T audio_data_buf
typedef struct T *T;

extern T create_audio_data_buf(int buf_size);
extern void free_audio_data_buf(T *bs);

extern unsigned int hget_buf_size(T buf);
extern void hputbuf(T buf, unsigned int val, int N);
extern unsigned long hsstell(T buf);
extern unsigned long hgetbits(T buf, int N);
extern unsigned int hget1bit(T buf);
extern void rewindNbits(T buf, int N);
extern void rewindNbytes(T buf, int N);

#undef T

#endif /* audio_data_buf_h */
