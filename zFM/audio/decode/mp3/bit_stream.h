//
//  bit_stream.h
//  mp3
//
//  Created by zykhbl on 16/3/10.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef bit_stream_h
#define bit_stream_h

#define T bit_stream
typedef struct T *T;

extern T create_bit_stream(char *bs_filenam, int size);
extern void free_bit_stream(T *bs);

extern int	end_bs(T bs);
extern unsigned long sstell(T bs);
extern unsigned int get1bit(T bs);
extern unsigned long getbits(T bs, int N);

#undef T

#endif /* bit_stream_h */
