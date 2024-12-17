#ifndef _H264_FILE_H_
#define _H264_FILE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct h264_file
{
    FILE* file;
    uint8_t* buffer;
    uint32_t buffer_size;
    uint32_t bytes_used;
    uint32_t count;
}h264_file_t;

int  h264file_open(h264_file_t* h264file, char* filename);
void  h264file_close(h264_file_t* h264file);
bool h264file_isopen(h264_file_t* h264file);
void  h264file_rewind(h264_file_t* h264file);
int  h264file_read_frame(h264_file_t* h264file, uint8_t* buf, uint32_t buf_size, bool *end);

#endif //!_H264_FILE_H_