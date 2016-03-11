/**********************************************************************
 Copyright (c) 1991 MPEG/audio software simulation group, All Rights Reserved
 huffman.c
 **********************************************************************/
/**********************************************************************
 * MPEG/audio coding/decoding software, work in progress              *
 *   NOT for public distribution until verified and approved by the   *
 *   MPEG/audio committee.  For further information, please contact   *
 *   Chad Fogg email: <cfogg@xenon.com>                               *
 *                                                                    *
 * VERSION 2.10                                                       *
 *   changes made since last update:                                  *
 *   date   programmers                comment                        *
 *27.2.92   F.O.Witte                  (ITT Intermetall)              *
 *                     email: otto.witte@itt-sc.de    *
 *                     tel:   ++49 (761)517-125       *
 *                     fax:   ++49 (761)517-880       *
 *12.6.92   J. Pineda                  Added sign bit to decoder.     *
 * 08/24/93 M. Iwadare                 Changed for 1 pass decoding.   *
 *--------------------------------------------------------------------*
 *  7/14/94 Juergen Koller      Bug fixes in Layer III code           *
 *********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "huffman.h"

#define MXOFF	250

#define T huffcodetab

HUFFBITS dmask = (HUFFBITS)1 << (sizeof(HUFFBITS) * 8 - 1);

//array of all huffcodtable headers
//0..31 Huffman code table 0..31
//32,33 count1-tables
struct T ht[HTN];

//read the huffman decoder table
int read_decoder_table(FILE *fi) {
    int n, i, nn, t;
    unsigned int v0, v1;
    char command[100], line[100];
    for (n = 0; n < HTN; n++) {//table number treelen xlen ylen linbits
        do {
            fgets(line, 99, fi);//读取一行，huffdec.txt文件里一行最多70多个字节，所以不会出现截断一行的问题
        } while ((line[0] == '#') || (line[0] < ' '));//去掉注释或空白行（空白行时，line[0] = '\n'，为换行符）
        
        //把line中的数据按照格式读入command 和 ht[n]中
        sscanf(line, "%s %s %u %u %u %u", command, ht[n].tablename, &ht[n].treelen, &ht[n].xlen, &ht[n].ylen, &ht[n].linbits);
        
        if (strcmp(command, ".end") == 0) {//huffdec.txt文件结尾
            return n;
        } else if (strcmp(command, ".table") != 0) {
            fprintf(stderr, "huffman table %u data corrupted\n", n);
            return -1;
        }
        ht[n].linmax = (1 << ht[n].linbits) - 1;
        
        sscanf(ht[n].tablename, "%u", &nn);
        if (nn != n) {//当前的ht索引 n 和 huffdec.txt文件里读取的索引 不一致时
            fprintf(stderr, "wrong table number %u\n", n);
            return -2;
        }
        
        do {
            fgets(line, 99, fi);
        } while ((line[0] == '#') || (line[0] < ' '));
        
        sscanf(line, "%s %u", command, &t);//形如 .treedata 或 .reference 24
        
        if (strcmp(command, ".reference") == 0) {//引用前面的ht
            ht[n].ref = t;
            ht[n].val = ht[t].val;
            ht[n].treelen  = ht[t].treelen;
            if ((ht[n].xlen != ht[t].xlen) || (ht[n].ylen != ht[t].ylen)) {
                fprintf(stderr,"wrong table %u reference\n", n);
                return -3;
            };
            while ((line[0] == '#') || (line[0] < ' ')) {//这段while循环代码不会执行，可考虑去掉，因为line ＝ '.reference x'，即：line[0] == '.'
                fgets(line, 99, fi);
            }
        } else if (strcmp(command, ".treedata") == 0) {
            ht[n].ref = -1;
            ht[n].val = (unsigned char (*)[2])calloc(2 * (ht[n].treelen), sizeof(unsigned char));//ht[n].treelen(行) * 2(列) 的2维数组， 初始化为全0
            
            if ((ht[n].val == NULL) && (ht[n].treelen != 0)) {//calloc分配内存出错
                fprintf(stderr, "heaperror at table %d\n", n);
                exit(-10);
            }
            
            for (i = 0; i < ht[n].treelen; i++) {//把huffdec.txt文件里的值写入2维数组
                fscanf(fi, "%x %x", &v0, &v1);//以16进制读入整数，所以 ht[n].val 中的值为一个无符号char，范围为 [0, 255]
                ht[n].val[i][0] = (unsigned char)v0;
                ht[n].val[i][1] = (unsigned char)v1;
            }
            
            fgets(line, 99, fi);//read the rest of the line(读取一行的剩余部分，比如换行符'\n'等没用的字节)
        } else {
            fprintf(stderr, "huffman decodertable error at table %d\n", n);
        }
    }
    return n;
}

//do the huffman-decoding
//note! for counta,countb -the 4 bit value is returned in y, discard x
int huffman_decoder(T h, int *x, int *y, int *v, int *w, audio_data_buf buf) {
    HUFFBITS level;
    int point = 0;
    int error = 1;
    level = dmask;
    if (h->val == NULL) {
        return 2;
    }
    
    if (h->treelen == 0) {//table 0 needs no bits
        *x = *y = 0;
        return 0;
    }
    
    //Lookup in Huffman table
    do {//搜索二叉树，至到找到叶子节点
        if (h->val[point][0] == 0) {//end of tree(叶子节点：val[point][0] == 0，val[point][1]为量化系数)
            *x = h->val[point][1] >> 4;//h->val[point][1] 的高4位，量化系数残差谱线幅值范围为[0, 15]
            *y = h->val[point][1] & 0xf;//h->val[point][1] 的低4位，量化系数残差谱线幅值范围为[0, 15]
            
            error = 0;
            break;
        }
        if (hget1bit(buf)) {//左子树
            while (h->val[point][1] >= MXOFF) {//16进制时，MXOFF = 'fa' ??
                point += h->val[point][1];
            }
            point += h->val[point][1];
        } else {//右子树
            while (h->val[point][0] >= MXOFF) {//16进制时，MXOFF = 'fa' ??
                point += h->val[point][0];
            }
            point += h->val[point][0];
        }
        level >>= 1;
    } while (level || (point < ht->treelen));
    
    //Check for error
    if (error) {//set x and y to a medium value as a simple concealment
        printf("Illegal Huffman code in data.\n");
        *x = (h->xlen - (1 << 1));
        *y = (h->ylen - (1 << 1));
    }
    
    //为了使各组量化频谱系数所需的比特数最少，无噪声编码把一组576个量化频谱系数分成3个region（由低频到高频分别为big_value区，count1区，zero区），每个region一个霍夫曼码书
    
    //Process sign encodings for quadruples tables（count1区一个huffman码字表示4个量化系数，一共使用了2本码书）
    if (h->tablename[0] == '3' && (h->tablename[1] == '2' || h->tablename[1] == '3')) {
        *v = (*y >> 3) & 1;
        *w = (*y >> 2) & 1;
        *x = (*y >> 1) & 1;
        *y = *y & 1;
        
        //v, w, x and y are reversed in the bitstream
        //switch them around to make test bistream work
        
        //在huffman码字后是每个非零残差谱线的符号位
        if (*v) {
            if (hget1bit(buf) == 1) {
                *v = -*v;
            }
        }
        if (*w) {
            if (hget1bit(buf) == 1) {
                *w = -*w;
            }
        }
        if (*x) {
            if (hget1bit(buf) == 1) {
                *x = -*x;
            }
        }
        if (*y) {
            if (hget1bit(buf) == 1) {
                *y = -*y;
            }
        }
    } else {//Process sign and escape encodings for dual tables（big_value区一个huffman码字表示2个量化系数，使用32个huffman表）
        
        //x and y are reversed in the test bitstream
        //Reverse x and y here to make test bitstream work

        if (h->linbits) {// ??
            if ((h->xlen - 1) == *x) {
                *x += hgetbits(buf, h->linbits);
            }
        }
        if (*x) {//在huffman码字后是每个非零残差谱线的符号位
            if (hget1bit(buf) == 1) {
                *x = -*x;
            }
        }
        if (h->linbits) {// ??
            if ((h->ylen-1) == *y) {
                *y += hgetbits(buf, h->linbits);
            }
        }
        if (*y) {//在huffman码字后是每个非零残差谱线的符号位
            if (hget1bit(buf) == 1) {
                *y = -*y;
            }
        }
    }
    
    //zero区不用解码。表示余下子带谱线值全为0
    
    return error;    
}

#undef T

