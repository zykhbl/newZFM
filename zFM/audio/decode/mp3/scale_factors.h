//
//  scale_factors.h
//  mp3
//
//  Created by zykhbl on 16/3/9.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef scale_factors_h
#define scale_factors_h

#include "side_info.h"
#include "audio_data_buf.h"

typedef struct {
    int l[23];			//[cb]
    int s[3][13];		//[window][cb]
} III_scalefac_t[2];	//[ch]

extern void III_get_scale_factors(audio_data_buf buf, III_scalefac_t *scalefac, III_side_info si, int gr, int ch, frame fr_ps);

#endif /* scale_factors_h */
