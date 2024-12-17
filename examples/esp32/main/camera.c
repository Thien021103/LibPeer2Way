#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_camera.h"
#include "esp_timer.h"

#include "peer_connection.h"

extern PeerConnection* g_pc;
extern int gDataChannelOpened;
extern PeerConnectionState eState;
extern SemaphoreHandle_t xSemaphore;
extern int get_timestamp();
static const char* TAG = "Camera";

 // ESP32-CAM with OV2640
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

static camera_config_t camera_config = {

    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_pclk = CAM_PIN_PCLK,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .xclk_freq_hz = 20000000,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_VGA,
    .jpeg_quality = 10,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY};

esp_err_t camera_init() {
  // initialize the camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera Init Failed");
    return err;
  }

  return ESP_OK;
}

void camera_task(void* pvParameters) {
  static int fps = 0;
  static int64_t last_time;
  int64_t curr_time;

  camera_fb_t* fb = NULL;

  ESP_LOGI(TAG, "Camera Task Started");

  last_time = get_timestamp();

  for (;;) {
    if ((eState == PEER_CONNECTION_COMPLETED) && gDataChannelOpened) {
      fb = esp_camera_fb_get();

      if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
      }

      // ESP_LOGI(TAG, "Camera captured. size=%zu, timestamp=%llu", fb->len, fb->timestamp);
      if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
        peer_connection_datachannel_send(g_pc, (char*)fb->buf, fb->len);
        xSemaphoreGive(xSemaphore);
      }

      fps++;

      if ((fps % 100) == 0) {
        curr_time = get_timestamp();
        ESP_LOGI(TAG, "Camera FPS=%.2f", 1000.0f / (float)(curr_time - last_time) * 100.0f);
        last_time = curr_time;
      }

      esp_camera_fb_return(fb);
    }

    vTaskDelay(pdMS_TO_TICKS(1000 / 20));
  }
}
