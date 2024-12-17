#include <string.h>
#include "h264_file.h"

int  h264file_open(h264_file_t* h264file, char* filename)
{
    h264file->file = fopen(filename, "rb");
	if(h264file->file == NULL) {      
		return -1;
	}
    h264file->count = 0;
	h264file->bytes_used = 0;
    
    //get file size
    fseek(h264file->file, 0L, SEEK_END); 
  
    // calculating the size of the file 
    uint32_t size = ftell(h264file->file); 

    // go to the start
    fseek(h264file->file, 0, SEEK_SET); 

    h264file->buffer = malloc(size);
    if(!h264file->buffer)
    {
        fclose(h264file->file);
		h264file->file = NULL;
        return -1;
    }
    h264file->buffer_size = size;

	return 0;
}
void  h264file_close(h264_file_t* h264file)
{
    if(h264file && h264file->buffer)
    {
        free(h264file->buffer);
    }
    if(h264file->file) {
		fclose(h264file->file);
		h264file->file = NULL;
		h264file->count = 0;
		h264file->bytes_used = 0;
	}
}
void h264file_rewind(h264_file_t* h264file)
{
    if(h264file && h264file->file)
    {
        h264file->count = 0;
	    h264file->bytes_used = 0;
    }
}
bool h264file_isopen(h264_file_t* h264file)
{
    return (h264file->file != NULL);
}
int  h264file_read_frame(h264_file_t* h264file, uint8_t* buf, uint32_t buf_size, bool *end)
{
    if(h264file == NULL || h264file->file == NULL) 
    {
        printf("h264 file is not opened\n");
		return -1;
	}
    
	int bytes_read = (int)fread(h264file->buffer, 1, h264file->buffer_size, h264file->file);

	if(bytes_read == 0) 
    {
		printf("Set to the beginning of file\n");
		fseek(h264file->file, 0, SEEK_SET); 
		h264file->count = 0;
		h264file->bytes_used = 0;
		bytes_read = (int)fread(h264file->buffer, 1, buf_size, h264file->file);
		if(bytes_read == 0)         
        {            
			h264file_close(h264file);
			return -1;
		}
	}

	bool is_find_start = false, is_find_end = false;
	int i = 0, start_code = 3;
	*end = false;

	for (i=0; i<bytes_read-5; i++) 
    {
		if(h264file->buffer[i] == 0 && h264file->buffer[i+1] == 0 && h264file->buffer[i+2] == 1) {
			start_code = 3;
		}
		else if(h264file->buffer[i] == 0 && h264file->buffer[i+1] == 0 && h264file->buffer[i+2] == 0 && h264file->buffer[i+3] == 1) {
			start_code = 4;
		}
		else  {
			continue;
		}
		if (((h264file->buffer[i+start_code]&0x1F) == 0x5 || (h264file->buffer[i+start_code]&0x1F) == 0x1) 
			&& ((h264file->buffer[i+start_code+1]&0x80) == 0x80)) {

			is_find_start = true;
			i += 4;
			break;
		}
	}

	for (; i<bytes_read-5; i++) 
    {
		if(h264file->buffer[i] == 0 && h264file->buffer[i+1] == 0 && h264file->buffer[i+2] == 1)
		{
			start_code = 3;
		}
		else if(h264file->buffer[i] == 0 && h264file->buffer[i+1] == 0 && h264file->buffer[i+2] == 0 && h264file->buffer[i+3] == 1) {
			start_code = 4;
		}
		else   {
			continue;
		}
        
		if (((h264file->buffer[i+start_code]&0x1F) == 0x7) || ((h264file->buffer[i+start_code]&0x1F) == 0x8) ||
			((h264file->buffer[i+start_code]&0x1F) == 0x6) || 
			(((h264file->buffer[i+start_code]&0x1F) == 0x5 || (h264file->buffer[i+start_code]&0x1F) == 0x1) && 
			((h264file->buffer[i+start_code+1]&0x80) == 0x80)))  {
			
			is_find_end = true;
			break;
		}
	}

	bool flag = false;
	if(is_find_start && !is_find_end && h264file->count > 0) {        
		flag = is_find_end = true;
		i = bytes_read;
		*end = true;
	}

	if(!is_find_start || !is_find_end) {
		h264file_close(h264file);
		return -1;
	}

	int size = (i <= buf_size ? i : buf_size);
	memcpy(buf, h264file->buffer, size); 
	
	if(!flag) {
		h264file->count += 1;
		h264file->bytes_used += i;
	}
	else {
		h264file->count = 0;
		h264file->bytes_used = 0;
	}
	
	fseek(h264file->file, h264file->bytes_used, SEEK_SET);
	return size;
}

void  send_frame(h264_file_t* h264file, uint8_t* buf, uint16_t buf_size, bool *end)
{
    
}