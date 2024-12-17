
#include "streamout.h"

#if defined(INPUT_SOURCE) && (INPUT_SOURCE == USE_FILE)

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <ctype.h>

#include "utils.h"

#include "h264_file.h"
#include "define.h"
#include "udp_utils.h"

#define SEND_FRAME_BY_FRAME 1
#define VIDEO_I_FRAME 1
#define VIDEO_P_FRAME 0

 char online_client_num;
 int running = 1;

char gVideoFn[128] = "1080.bin";
char gAudioFn[128] = "1080.raw";

int is_stop = 1;

static udp_t udp = {0};

static unsigned long long getTimeStamp()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

static int StreamoutSendVideo(unsigned long long timestamp, uint8_t* buf, int size, int frameType)
{
    int i = 0, av_channel_id = 0, video_enable = 0, ret = 0, lock_ret = 0;
    int wait_key_frame = 0;
    uint8_t *videoBuf   = buf;
    int videoSize    = size;
    int sendFrameOut = 0;

    // *** set Video Frame info here ***
	// Send Video Frame to av-idx and know how many time it takes
	//TODO calling send function

	#if defined(UDP_ENABLE) && (UDP_ENABLE == 1)
	ret = udp_send_frame(&udp, videoBuf, videoSize);
	#endif
	return 0;
}

static int StreamoutSendAudio(unsigned long long timestamp, char* buf, int size)
{
    //TODO calling send function

    return 0;
}

/********
Thread - Send Audio frames to all AV-idx
*/
static void *thread_AudioFrameData(void *arg)
{
	FILE *fp = NULL;
	char buf[AUDIO_BUF_SIZE];
	int frameRate = AUDIO_FPS;
	int sleepTick = 1000000/frameRate;

	fp = fopen(gAudioFn, "rb");
	if(fp == NULL)
	{
		printf("thread_AudioFrameData: exit\n");
		pthread_exit(0);
	}

	while(running)
	{
		int size = fread(buf, 1, AUDIO_FRAME_SIZE, fp);
		if(size <= 0)
		{
			printf("rewind audio\n");
			rewind(fp);
			continue;
		}

		StreamoutSendAudio(getTimeStamp(), buf, size);

		usleep(sleepTick);
	}

	fclose(fp);

	printf("[thread_AudioFrameData] exit\n");

	pthread_exit(0);
}

////////////////////////////////////////////////////
static void *thread_SendVideoFrameByFrame(void *arg)
{
	uint8_t buf[VIDEO_BUF_SIZE];
	int ret = 0;
	uint32_t size = 0, bSendFrameOut = 0;
	bool end = 0;
	int fpsCnt = 0, round = 0, framerate = VIDEO_FPS, sleepTick = 1000000/framerate;
	struct timeval tStart, tEnd;
	long takeSec = 0, takeUSec = 0, sendFrameRoundTick = 0;
	int flags = 0;
	h264_file_t h264file;
	memset(&h264file, 0, sizeof(h264_file_t));

	ret = h264file_open(&h264file, gVideoFn);
	if(ret < 0)
	{
		printf("Cannot open file video %s\n", gVideoFn);
		return NULL;
	}


	LOGI("thread_VideoFrameData start OK. running = %d\n", running);
	
	while(running)
	{
		// if no connection ==> sleep some ms to wait
		if(online_client_num <= 0)
		{
			usleep(10 * 1000);
			continue;
		}
		takeSec = 0; takeUSec = 0;
		sendFrameRoundTick = 0;
		gettimeofday(&tStart, NULL);

		size = h264file_read_frame(&h264file, buf, VIDEO_BUF_SIZE, &end);
		// printf("h264file_read_frame: size = %d\n", size);
		
		if(size > 0)
		{
			int nal_type_pos = 0;

			if(buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01)
			{
				nal_type_pos = 3;
			}
			else if(buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x01)
			{
				nal_type_pos = 4;
			}
			else
			{
				printf("Error NAL\n");
			}

			if(nal_type_pos > 0)
			{
				if((buf[nal_type_pos] == 0x67) || (buf[nal_type_pos] == 0x68) || (buf[nal_type_pos] == 0x65))
				{
					//This is key frame
					flags = VIDEO_I_FRAME;
				}
				else
				{
					//This is P frame
					flags = VIDEO_P_FRAME;
				}
			}
			uint64_t timestamp = getTimeStamp();
			bSendFrameOut = StreamoutSendVideo(timestamp, buf, size, flags);
			
		}
		
		//sleep
		gettimeofday(&tEnd, NULL);
		takeSec = tEnd.tv_sec-tStart.tv_sec;
		takeUSec = tEnd.tv_usec-tStart.tv_usec;
		if(takeUSec < 0) {
			takeSec--;
			takeUSec += 1000000;
		}
		sendFrameRoundTick += takeUSec;
		
		if( sleepTick > sendFrameRoundTick )
            usleep(sleepTick-sendFrameRoundTick);

		if(end)
		{
			printf("Rewind video\n");
			h264file_rewind(&h264file);
			usleep(33 * 1000);
		}
	}

__exit:
	h264file_close(&h264file);
	
	pthread_exit(0);

	printf("thread_SendVideoFrameByFrame is terminated!\n");
}

int create_streamout_thread(int argc, char *argv[])
{
    pthread_t ThreadVideoFrameData_ID;
    pthread_t ThreadAudioFrameData_ID;
    int ret = 0;
	running = 1;
	online_client_num = 1;
	is_stop = 0;
	// LOGI("WE came here");
	#if defined(UDP_ENABLE) && (UDP_ENABLE == 1)
    ret = udp_sock_create(&udp, UDP_PORT, UDP_DEST_ADDR);
    if(ret != 0) printf("Error: Can not create udp...\n");
    #endif

	// create thread to send video frame
	if((ret = pthread_create(&ThreadVideoFrameData_ID, NULL, &thread_SendVideoFrameByFrame, NULL)))
	{
		printf("pthread_create ret=%d\n", ret);
		return -1;
	}
	pthread_detach(ThreadVideoFrameData_ID);

	// create thread to send audio frame
	// if((ret = pthread_create(&ThreadAudioFrameData_ID, NULL, &thread_AudioFrameData, NULL)))
	// {
	// 	printf("pthread_create ret=%d\n", ret);
	// 	return -1;
	// }
	// pthread_detach(ThreadAudioFrameData_ID);
    
    return 0;
}

int stop_streamout_thread()
{
	is_stop = 1;
    return 0;
}

int is_stop_streamout_thread()
{
    return is_stop;
}

#endif //INPUT_SOURCE