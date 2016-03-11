//
//  common.c
//  mp3
//
//  Created by zykhbl on 16/3/10.
//  Copyright © 2016年 zykhbl. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

void *mem_alloc(unsigned long block, char *item) {
    void *ptr;
    ptr = (void *)malloc((unsigned long)block);
    if (ptr != NULL) {
        memset(ptr, 0, block);
    } else {
        printf("Unable to allocate %s\n", item);
        exit(0);
    }
    return ptr;
}

FILE *openTableFile(char *name) {
    char fulname[80];
    FILE *f;
    
    fulname[0] = '\0';
    
    strcat(fulname, name);
    if( (f = fopen(fulname, "r")) == NULL ) {
        fprintf(stderr,"\nopenTable: could not find %s\n", fulname);
    }
    
    return f;
}