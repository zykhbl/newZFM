//
//  common.h
//  mp3Decoder
//
//  Created by zykhbl on 16/2/24.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#ifndef common_h
#define common_h

//MPEG Header Definitions - Mode Values
#define	MPG_MD_STEREO           0
#define	MPG_MD_JOINT_STEREO     1
#define	MPG_MD_DUAL_CHANNEL     2
#define	MPG_MD_MONO             3

#define	SYNC_WORD			(long)0xfff
#define	SYNC_WORD_LENGTH	12

#define	ALIGNING			8

#define	MINIMUM				4  // Minimum size of the buffer in bytes
#define	MAX_LENGTH			32 //Maximum length of word written or read from bit stream

#define	BINARY				0  //Binary input file
#define	READ_MODE			0  //Decode mode only

#define	FALSE				0
#define	TRUE				1

#define	MIN(A, B)			((A) < (B) ? (A) : (B))

#define	SBLIMIT				32
#define	SSLIMIT				18
#define	BUFFER_SIZE			4096
#define	HAN_SIZE			512
#define	SCALE				32768

#define	PI					3.14159265358979
#define	PI64				PI/64
#define	PI4					PI/4

extern int putmask[9];

extern void *mem_alloc(unsigned long block, char *item);
extern FILE *openTableFile(char *name);

#endif
