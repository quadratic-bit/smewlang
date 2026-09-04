#ifndef BUF_H
#define BUF_H

#include <stdio.h>

#include "vec.h"

typedef enum {BUF_OK, BUF_ERR} BufResult;

typedef Vec(char) SourceBuffer;

BufResult buf_read(SourceBuffer *buf, FILE *input_file);

#endif
