#include <arpa/inet.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h> // Include this
#include <gst/app/gstappsrc.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "peer.h"

//const char CAMERA_PIPELINE[] = "v4l2src device=/dev/video0 ! video/x-raw,format=YUY2,width=640,height=480 , framerate=30/1 ! videoconvert ! video/x-raw,format=I420 ! x264enc tune=zerolatency bitrate=500 speed-preset=ultrafast ! h264parse config-interval=-1 ! video/c-h264, stream-format=byte-stream ! appsink name=camera-sink";
//const char CAMERA_PIPELINE[] = "v4l2src device=/dev/video0 ! videoconvert ! video/x-raw,format=I420,width=640,height=480,framerate=30/1 ! x264enc bitrate=500 speed-preset=ultrafast ! h264parse config-interval=-1 ! video/x-h264,stream-format=byte-stream ! appsink name=camera-sink";
const char CAMERA_PIPELINE[] = "v4l2src device=/dev/video0 ! queue ! video/x-raw, width=640, height=480, framerate=30/1 ! queue ! videoconvert ! queue ! video/x-raw, format=I420 ! queue ! x264enc tune=fastdecode speed-preset=ultrafast bitrate=1000 key-int-max=15 ! queue ! h264parse config-interval=10 ! queue ! video/x-h264, stream-format=byte-stream ! queue ! appsink name=camera-sink sync=false max-buffers=1 drop=true";

//v4l2src device="/dev/video" ! queue ! v4l2convert ! video/x-raw,format=I420,width=640,height=480,framerate=30/1 ! v4l2h264enc ! h264parse ! video/x-h264,stream-format=byte-stream,alignment=au,width=640,height=480,framerate=30/1,profile=baseline,level=(string)4 ! appsink sync=TRUE emit-signals=TRUE name=appsink-video

// const char MIC_PIPELINE[] = "alsasrc device=plughw:seeed2micvoicec,0 ! audio/x-raw,format=S16LE,rate=8000,channels=1 ! alawenc ! appsink name=mic-sink";
const char MIC_PIPELINE[] = "alsasrc latency-time=20000 device=plughw:0,6 ! audio/x-raw,format=S16LE,rate=8000,channels=1 ! alawenc ! appsink name=mic-sink";

const char SPK_PIPELINE[] = "appsrc name=spk-src format=time ! audio/x-alaw ! alawdec ! audio/x-raw,format=S16LE,rate=8000,channels=1 ! alsasink sync=false device=plughw:0,0";
//const char SPK_PIPELINE[] = "appsrc name=spk-src format=time ! alawdec ! audio/x-raw,format=S16LE,rate=8000,channels=1 ! alsasink sync=false device=plughw:seeed2micvoicec,0";

int g_interrupted = 0;
PeerConnection* g_pc = NULL;
PeerConnectionState g_state;

typedef struct Media {
  // Camera elements
  GstElement* camera_pipeline;
  GstElement* camera_sink;

  // Microphone elements
  GstElement* mic_pipeline;
  GstElement* mic_sink;

  // Speaker elements
  GstElement* spk_pipeline;
  GstElement* spk_src;

} Media;

Media g_media;

static uint64_t get_timestamp();

static void onconnectionstatechange(PeerConnectionState state, void* data) {
    printf("state is changed: %s\n", peer_connection_state_to_string(state));
    g_state = state;
    if (g_state == PEER_CONNECTION_CONNECTED) {
        gst_element_set_state(g_media.camera_pipeline, GST_STATE_PLAYING);
        gst_element_set_state(g_media.mic_pipeline, GST_STATE_PLAYING);
        gst_element_set_state(g_media.spk_pipeline, GST_STATE_PLAYING);
    } else if (g_state == PEER_CONNECTION_DISCONNECTED || g_state == PEER_CONNECTION_FAILED) {
        gst_element_set_state(g_media.camera_pipeline, GST_STATE_NULL);
        gst_element_set_state(g_media.mic_pipeline, GST_STATE_NULL);
        gst_element_set_state(g_media.spk_pipeline, GST_STATE_NULL);
    }
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

static GstFlowReturn on_video_data(GstElement* sink, void* data) {
  GstSample* sample;
  GstBuffer* buffer;
  GstMapInfo info;

  g_signal_emit_by_name(sink, "pull-sample", &sample);

  if (sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);
    peer_connection_send_video(g_pc, info.data, info.size);

    //printf(", send frame\n");

    gst_buffer_unmap(buffer, &info);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

static GstFlowReturn on_audio_data(GstElement* sink, void* data) {
  GstSample* sample;
  GstBuffer* buffer;
  GstMapInfo info;

  g_signal_emit_by_name(sink, "pull-sample", &sample);

  if (sample) {
    buffer = gst_sample_get_buffer(sample);

    gst_buffer_map(buffer, &info, GST_MAP_READ);
    peer_connection_send_audio(g_pc, info.data, info.size);
    gst_buffer_unmap(buffer, &info);

    gst_sample_unref(sample);
    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

static void onremoteaudio(uint8_t* data, size_t size, void *userdata) {
    g_print("Recv remote audio\n:%d\n",(int)size);

    GstBuffer *audio_buffer = gst_buffer_new_and_alloc(size);
    gst_buffer_fill(audio_buffer, 0, data, size);
    gst_app_src_push_buffer(GST_APP_SRC(g_media.spk_src), audio_buffer);
}

static void signal_handler(int signal) {
  g_interrupted = 1;
  peer_signaling_leave_channel();
}

static void on_request_keyframe(void* data) {
  printf("request keyframe\n");
}
// static void request_periodic_keyframe() {
//     while (!g_interrupted) {
//         on_request_keyframe(NULL);  // Request keyframe at intervals
//         sleep(1);  // Adjust as needed for your latency requirements
//     }
// }

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
  pthread_t peer_singaling_thread;
  pthread_t peer_connection_thread;

  signal(SIGINT, signal_handler);

  PeerConfiguration config = {
      .ice_servers = 
          {
            {.urls = "stun:stun.l.google.com:19302"},
            // {.urls = "turn:192.168.0.170:3478?transport=udp",
            //  .username="camera",
            //  .credential="camera123"
            // }
          },
      .datachannel = DATA_CHANNEL_STRING,
      .video_codec = CODEC_H264,
      .audio_codec = CODEC_PCMA,
      .onaudiotrack = onremoteaudio,
      .on_request_keyframe = on_request_keyframe
  };
      
  ServiceConfiguration service_config = SERVICE_CONFIG_DEFAULT();
  service_config.ws_url = "127.0.0.1";
  service_config.ws_port = 8000;


  printf("open https://sepfy.github.io/webrtc?deviceId=%s\n", argv[1]);

  gst_init(&argc, &argv);

  g_media.camera_pipeline = gst_parse_launch(CAMERA_PIPELINE, NULL);
  g_media.camera_sink = gst_bin_get_by_name(GST_BIN(g_media.camera_pipeline), "camera-sink");
  g_signal_connect(g_media.camera_sink, "new-sample", G_CALLBACK(on_video_data), NULL);
  g_object_set(g_media.camera_sink, "emit-signals", TRUE, NULL);

  g_media.mic_pipeline = gst_parse_launch(MIC_PIPELINE, NULL);
  g_media.mic_sink = gst_bin_get_by_name(GST_BIN(g_media.mic_pipeline), "mic-sink");
  g_signal_connect(g_media.mic_sink, "new-sample", G_CALLBACK(on_audio_data), NULL);
  g_object_set(g_media.mic_sink, "emit-signals", TRUE, NULL);

  g_media.spk_pipeline = gst_parse_launch(SPK_PIPELINE, NULL);
  g_media.spk_src = gst_bin_get_by_name(GST_BIN(g_media.spk_pipeline), "spk-src");
  g_object_set(g_media.spk_src, "emit-signals", TRUE, NULL);

  peer_init();

  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, onconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessage, onopen, onclose);

  service_config.client_id = argv[1];
  service_config.pc = g_pc;
  peer_signaling_set_config(&service_config);
  // peer_signaling_join_channel();

  pthread_create(&peer_connection_thread, NULL, peer_connection_task, NULL);
  pthread_create(&peer_singaling_thread, NULL, peer_singaling_task, NULL);

  while (!g_interrupted) {
    sleep(1);
  }

  gst_element_set_state(g_media.camera_pipeline, GST_STATE_NULL);
  gst_element_set_state(g_media.mic_pipeline, GST_STATE_NULL);
  gst_element_set_state(g_media.spk_pipeline, GST_STATE_NULL);

  pthread_join(peer_singaling_thread, NULL);
  pthread_join(peer_connection_thread, NULL);

  peer_signaling_leave_channel();
  peer_connection_destroy(g_pc);
  peer_deinit();

  return 0;
}

// Check dồng bộ time giữa audio và video
// Nếu đưa xuống mạch thì có dùng được gst không
// Nguồn phát từ đâu
// Thiếu 1 laptop
// Tính năng 2 chiều
// Viết full luồng nhận
