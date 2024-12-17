#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h> // Include this
#include <gst/app/gstappsrc.h>

#include "peer.h"
// #include "streamout.h"
#include "reader.h"

int g_interrupted = 0;
PeerConnection* g_pc = NULL;
PeerConnectionState g_state;//alsasink sync=true device=plughw:1,0

const char SPK_PIPELINE[] = "appsrc name=spk-src format=time ! audio/x-alaw ! alawdec ! audio/x-raw,format=S16LE,rate=8000,channels=1 ! alsasink sync=false device=plughw:0,0";

typedef struct Media {
  // Speaker elements
  GstElement* spk_pipeline;
  GstElement* spk_src;

} Media;

Media g_media;

static void onconnectionstatechange(PeerConnectionState state, void* data) {
  printf("state is changed: %s\n", peer_connection_state_to_string(state));
  g_state = state;
}

static void onopen(void* user_data) {
}

static void onclose(void* user_data) {
}

static void onmessage(char* msg, size_t len, void* user_data, uint16_t sid) {
  printf("on message: %d %.*s", sid, (int)len, msg);

  if (strncmp(msg, "ping", 4) == 0) {
    printf(", send pong\n");
    peer_connection_datachannel_send(g_pc, "pong", 4);
  }
}

static void onremoteaudio(uint8_t* data, size_t size, void *userdata) {
    // g_print("Recv remote audio\n:%d\n",(int)size);

    GstBuffer *audio_buffer = gst_buffer_new_and_alloc(size);
    gst_buffer_fill(audio_buffer, 0, data, size);
    gst_app_src_push_buffer(GST_APP_SRC(g_media.spk_src), audio_buffer);
}

static void signal_handler(int signal) {
  g_interrupted = 1;
  peer_signaling_leave_channel();
}

static void* peer_singaling_task(void* data) {

//  while (!g_interrupted) {
//    peer_signaling_loop();
//    usleep(1000);
      connect_to_janus_server();
//  }

    pthread_exit(NULL);
}

static void* peer_connection_task(void* data) {
  while (!g_interrupted) {
    peer_connection_loop(g_pc);
    usleep(1000);
  }

  pthread_exit(NULL);
}

static uint64_t get_timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int main(int argc, char* argv[]) {
  uint64_t curr_time, video_time, audio_time;
  uint8_t buf[524288];
  int size;

  gst_init(&argc, &argv);

  g_media.spk_pipeline = gst_parse_launch(SPK_PIPELINE, NULL);
    g_media.spk_src = gst_bin_get_by_name(GST_BIN(g_media.spk_pipeline), "spk-src");
    g_object_set(g_media.spk_src, "emit-signals", TRUE, NULL);

  pthread_t peer_singaling_thread;
  pthread_t peer_connection_thread;

  signal(SIGINT, signal_handler);

  PeerConfiguration config = {
      .ice_servers = {
         {.urls = "stun:stun.l.google.com:19302"},
          // {.urls = "turn:192.168.0.170:3478?transport=udp",
          //  .username="camera",
          //  .credential="camera123"}
      },
      .datachannel = DATA_CHANNEL_STRING,
      .video_codec = CODEC_H264,
      .audio_codec = CODEC_PCMA,
       .onaudiotrack = onremoteaudio};


  ServiceConfiguration service_config = SERVICE_CONFIG_DEFAULT();
  service_config.ws_url = "192.168.0.169";
  service_config.ws_port = 8000;

  peer_init();
  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, onconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);


  service_config.client_id = "1001";
  service_config.pc = g_pc;
  peer_signaling_set_config(&service_config);
//  peer_signaling_join_channel();

  pthread_create(&peer_connection_thread, NULL, peer_connection_task, NULL);
  pthread_create(&peer_singaling_thread, NULL, peer_singaling_task, NULL);
//  printf("Connect to Janus\n");
//  connect_to_janus_server();

  reader_init();

  gboolean streamout = FALSE;
  gboolean isReceiveAudio = FALSE;

  while (!g_interrupted) {
    if (g_state == PEER_CONNECTION_COMPLETED) {
      // if(!streamout) {
      //     create_streamout_thread(argc, argv);
      //     streamout = TRUE;
      // }
      curr_time = get_timestamp();

      if (!isReceiveAudio) {
          isReceiveAudio = TRUE;
          gst_element_set_state(g_media.spk_pipeline, GST_STATE_PLAYING);
      }

      // FPS 25
      if (curr_time - video_time > 9) {
          video_time = curr_time;
          if (reader_get_video_frame(buf, &size) == 0) {
            peer_connection_send_video(g_pc, buf, size);
        }
      }

      if (curr_time - audio_time > 20) {
        if (reader_get_audio_frame(buf, &size) == 0) {
          peer_connection_send_audio(g_pc, buf, size);
        }
        audio_time = curr_time;
      }

      usleep(1000);
    }
  }

  pthread_join(peer_singaling_thread, NULL);
  pthread_join(peer_connection_thread, NULL);

  // stop_streamout_thread();
  // streamout = FALSE;
  reader_deinit();

  peer_signaling_leave_channel();
  peer_connection_destroy(g_pc);
  peer_deinit();

  return 0;
}
