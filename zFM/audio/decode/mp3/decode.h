//
//  decode.h
//  mp3Decoder
//
//  Created by zykhbl on 16/2/24.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef decode_h
#define decode_h

#include "scale_factors.h"

extern void III_hufman_decode(audio_data_buf buf, long int is[SBLIMIT][SSLIMIT], III_side_info si, int ch, int gr, int part2_start, frame fr_ps);
extern void III_dequantize_sample(long int is[SBLIMIT][SSLIMIT], double xr[SBLIMIT][SSLIMIT], III_scalefac_t *scalefac, struct gr_info_s *gr_info, int ch, frame fr_ps);
extern void III_reorder(double xr[SBLIMIT][SSLIMIT], double ro[SBLIMIT][SSLIMIT], struct gr_info_s *gr_info, frame fr_ps);
extern void III_stereo(double xr[2][SBLIMIT][SSLIMIT], double lr[2][SBLIMIT][SSLIMIT], III_scalefac_t *scalefac, struct gr_info_s *gr_info, frame fr_ps);
extern void III_antialias(double xr[SBLIMIT][SSLIMIT], double hybridIn[SBLIMIT][SSLIMIT], struct gr_info_s *gr_info, frame fr_ps);
extern void III_hybrid(double fsIn[SSLIMIT], double tsOut[SSLIMIT], int sb, int ch, struct gr_info_s *gr_info, frame fr_ps);
extern int subBandSynthesis (double *bandPtr, int channel, short *samples);

#endif
