#ifndef _STREAM_OUT_H_
#define _STREAM_OUT_H_

#define USE_FILE    0
#define USE_WEBCAM  1
#define INPUT_SOURCE 0 // 0: FILE, 1: WEBCAM

#define ENBALE_SEND // enable send video & audio to client

// for stream video to udp and using ffplay to play video stream ffplay udp://127.0.0.1:11000
#define UDP_ENABLE 1 //enable send to udp
#define UDP_PORT 11000
#define UDP_DEST_ADDR "127.0.0.1"

int create_streamout_thread(int argc, char *argv[]);
int stop_streamout_thread();
int is_stop_streamout_thread();

#endif // _STREAM_OUT_H_