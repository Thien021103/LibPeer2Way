#ifndef _DEFINE_H_
#define _DEFINE_H_

#include <pthread.h>


#define IOTYPE_USER_START_LIVEVIEW_VIDEO	1
#define IOTYPE_USER_STOP_LIVEVIEW_VIDEO		2
#define IOTYPE_USER_START_LIVEVIEW_ADUIO	4
#define IOTYPE_USER_STOP_LIVEVIEW_AUDIO		5
#define IOTYPE_USER_START_SPEAKER			6
#define IOTYPE_USER_STOP_SPEAKER			7

#define MAX_CLIENT_NUMBER	8

#define VIDEO_BUF_SIZE	(1024l * 300l)
#define AUDIO_BUF_SIZE	1024

#define VIDEO_FPS 60   // Video FPS

#define AUDIO_FORMAT_PCM

#ifdef  AUDIO_FORMAT_PCM
#   define AUDIO_FRAME_SIZE 640
#   define AUDIO_FPS        25
#   define AUDIO_CODEC      0x8C
#else
#   error "Not define audio"
#endif


#endif