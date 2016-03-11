//
//  audio_data_buf.h
//  mp3
//
//  Created by zykhbl on 16/3/10.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef audio_data_buf_h
#define audio_data_buf_h

struct audio_data_buf {
    unsigned int    *buf;
    unsigned int    buf_size;
    unsigned int    buf_bit_idx;
    unsigned long   offset;
    unsigned long   totbit;
    unsigned long   buf_byte_idx;
};

extern struct audio_data_buf*create_audio_data_buf(int buf_size);
extern void free_audio_data_buf(struct audio_data_buf**bs);

extern unsigned int hget_buf_size(struct audio_data_buf*buf);
extern void hputbuf(struct audio_data_buf*buf, unsigned int val, int N);
extern unsigned long hsstell(struct audio_data_buf*buf);
extern unsigned long hgetbits(struct audio_data_buf*buf, int N);
extern unsigned int hget1bit(struct audio_data_buf*buf);
extern void rewindNbits(struct audio_data_buf*buf, int N);
extern void rewindNbytes(struct audio_data_buf*buf, int N);

#endif /* audio_data_buf_h */
