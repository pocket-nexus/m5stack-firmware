/* IMU window: the LSM6DS3TR-C fused by the factory's Mahony filter
 * (imu_view.c) on a 10 ms task, the textured device model rendered into the
 * texture "assets/imu-canvas.png" on a render task, the one- and two-finger
 * gestures and the finger markers (ui_function_imu_window.cpp). */
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu_view.h"
#include "m5_corep4x/bsp.h"
#include "m5_corep4x/imu.h"
#include "svc.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "svc_imu";

#define TEXTURE "assets/imu-canvas.png"
#define SIDE 512
/* The canvas's screen rectangle. */
#define CANVAS_X 10
#define CANVAS_Y 52
#define MARKER_HALF 14
#define SAMPLE_MS 10
/* The view is rendered at up to 30 Hz (the factory: 40 Hz) whatever the
 * frame rate: a render keeps a core busy for most of a 60 Hz frame. */
#define RENDER_PERIOD_US 33333

enum { STARTING = 0, CALIBRATING = 1, READY = 2, FAILED = 3 };

extern const uint8_t front_start[] asm("_binary_corep4x_front_rgb565_start");
extern const uint8_t back_start[] asm("_binary_corep4x_back_rgb565_start");
extern const uint8_t top_start[] asm("_binary_corep4x_top_rgb565_start");
extern const uint8_t bottom_start[] asm("_binary_corep4x_bottom_rgb565_start");
extern const uint8_t left_start[] asm("_binary_corep4x_left_rgb565_start");
extern const uint8_t right_start[] asm("_binary_corep4x_right_rgb565_start");

/* The window is open; generation of the visit (stale tasks stop). */
static atomic_bool s_open;
static atomic_uint s_generation;
static atomic_bool s_recenter;
static atomic_bool s_init_running;
static TaskHandle_t s_fusion;
static TaskHandle_t s_render;

/* The fusion task's latest relative pose. */
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static imu_quat_t s_relative;
static uint32_t s_pose_sequence;
static bool s_pose_ready;

/* Render hand-off: frame() asks for a render of `s_render_pose`, the render
 * task draws into s_pixels and sets s_rendered, frame() uploads it. */
enum { RENDER_IDLE, RENDER_BUSY, RENDER_DONE };
static atomic_int s_render_state;
static imu_quat_t s_render_pose;
static uint16_t *s_pixels;

/* App-task state. */
static imu_quat_t s_view;
static uint32_t s_view_sequence;
static uint32_t s_drawn_pose = UINT32_MAX;
static uint32_t s_drawn_view = UINT32_MAX;
static int64_t s_render_requested_us;
static imu_gesture_t s_gesture;
static int s_state = -1;

static void set_state(int state) {
  m5_pocket_bridge_set_number("imuState", state);
}

static void publish_pose(imu_quat_t q) {
  portENTER_CRITICAL(&s_lock);
  s_relative = q;
  s_pose_sequence++;
  s_pose_ready = true;
  portEXIT_CRITICAL(&s_lock);
}

/* imu_fusion: the factory's axis mapping (the package is turned 90 degrees
 * and the model has +Y up): a = (ay, ax, az), w = (-gy, -gx, -gz). */
static void fusion_task(void *arg) {
  const unsigned generation = (unsigned)(uintptr_t)arg;
  imu_attitude_t attitude;
  imu_attitude_reset(&attitude);
  imu_quat_t reference = imu_quat_identity();
  bool has_reference = false;
  float acc[3] = {0, 0, 1};
  float gyr[3] = {0, 0, 0};
  bool acc_seen = false;
  int64_t last_ms = -1;
  while (atomic_load(&s_open) && atomic_load(&s_generation) == generation) {
    m5_corep4x_imu_sample_t s;
    if (m5_corep4x_imu_read(&s) == ESP_OK && (s.acc_valid || s.gyr_valid) &&
        s.time_ms != last_ms) {
      float dt = 0.010f;
      if (last_ms >= 0 && s.time_ms - last_ms > 0 &&
          s.time_ms - last_ms <= 100) {
        dt = (float)(s.time_ms - last_ms) / 1000.0f;
      }
      last_ms = s.time_ms;
      if (s.acc_valid) {
        acc[0] = s.ax;
        acc[1] = s.ay;
        acc[2] = s.az;
        acc_seen = true;
      }
      if (s.gyr_valid) {
        gyr[0] = s.gx;
        gyr[1] = s.gy;
        gyr[2] = s.gz;
      }
      if (imu_attitude_update(&attitude, acc[1], acc[0], acc[2], s.acc_valid,
                              -gyr[1], -gyr[0], -gyr[2], s.gyr_valid, dt)) {
        bool recenter = true;
        if (imu_attitude_calibrated(&attitude) && acc_seen &&
            atomic_compare_exchange_strong(&s_recenter, &recenter, false) &&
            imu_attitude_recenter(&attitude, acc[1], acc[0], acc[2])) {
          reference = imu_attitude_orientation(&attitude);
          has_reference = true;
          publish_pose(imu_quat_identity());
        } else if (!has_reference && imu_attitude_calibrated(&attitude)) {
          imu_attitude_clear_correction(&attitude);
          reference = imu_attitude_orientation(&attitude);
          has_reference = true;
          publish_pose(imu_quat_identity());
          ESP_LOGI(TAG, "gyro bias calibrated; model revealed");
        } else if (has_reference) {
          publish_pose(imu_quat_normalize(
              imu_quat_multiply(imu_quat_conjugate(reference),
                                imu_attitude_orientation(&attitude))));
        }
      }
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SAMPLE_MS));
  }
  /* A later visit may have started its own fusion task already. */
  taskENTER_CRITICAL(&s_lock);
  if (s_fusion == xTaskGetCurrentTaskHandle()) {
    s_fusion = NULL;
  }
  taskEXIT_CRITICAL(&s_lock);
  vTaskDelete(NULL);
}

/* The sensor runs: "Keep sensor still / Calibrating IMU..." until the
 * first rendering is uploaded. */
static void start_fusion(unsigned generation) {
  TaskHandle_t task = NULL;
  if (xTaskCreate(fusion_task, "imu_fusion", 4096,
                  (void *)(uintptr_t)generation, 4, &task) == pdPASS) {
    taskENTER_CRITICAL(&s_lock);
    s_fusion = task;
    taskEXIT_CRITICAL(&s_lock);
  } else {
    m5_pocket_bridge_set_text("imuError", "ESP_ERR_NO_MEM");
    set_state(FAILED);
    return;
  }
  set_state(CALIBRATING);
}

/* imu_init_task: L3B on, the sensor initialised once, then fusion for the
 * visit that is open when the sensor is ready (one init task at a time;
 * a visit that opens meanwhile waits for it). */
static void init_task(void *arg) {
  (void)arg;
  const esp_err_t err = m5_corep4x_imu_init();
  atomic_store(&s_init_running, false);
  if (atomic_load(&s_open)) {
    if (err == ESP_OK) {
      start_fusion(atomic_load(&s_generation));
    } else {
      m5_pocket_bridge_set_text("imuError", esp_err_to_name(err));
      set_state(FAILED);
    }
  }
  vTaskDelete(NULL);
}

static void render_task(void *arg) {
  (void)arg;
  const imu_textures_t textures = {
      .front = (const uint16_t *)front_start,
      .back = (const uint16_t *)back_start,
      .top = (const uint16_t *)top_start,
      .bottom = (const uint16_t *)bottom_start,
      .left = (const uint16_t *)left_start,
      .right = (const uint16_t *)right_start,
  };
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (atomic_load(&s_render_state) != RENDER_BUSY || !s_pixels) {
      continue;
    }
    imu_render_device(s_pixels, SIDE, IMU_VIEW_WIDTH, IMU_VIEW_HEIGHT,
                      s_render_pose, &textures);
    atomic_store(&s_render_state, RENDER_DONE);
  }
}

static void hide_markers(void) { m5_pocket_bridge_set_number("imuMarkers", 0); }

/* update_touch_gesture, once per frame: up to two contacts inside the
 * canvas turn the view; markers sit on the fingers. */
static void update_gesture(void) {
  uint16_t xs[2];
  uint16_t ys[2];
  const size_t count = svc_contacts(xs, ys, 2);
  int16_t cx[2];
  int16_t cy[2];
  bool inside = count > 0;
  for (size_t i = 0; i < count; i++) {
    cx[i] = (int16_t)(xs[i] - CANVAS_X);
    cy[i] = (int16_t)(ys[i] - CANVAS_Y);
    if (cx[i] < 0 || cx[i] >= IMU_VIEW_WIDTH || cy[i] < 0 ||
        cy[i] >= IMU_VIEW_HEIGHT) {
      inside = false;
    }
  }
  if (!inside) {
    imu_gesture_reset(&s_gesture);
    hide_markers();
    return;
  }
  if (imu_gesture_update(&s_gesture, (int)count, cx, cy, &s_view)) {
    s_view_sequence++;
  }
  m5_pocket_bridge_set_number("imuMarkers", (double)count);
  m5_pocket_bridge_set_number("imuMarker0X", cx[0] - MARKER_HALF);
  m5_pocket_bridge_set_number("imuMarker0Y", cy[0] - MARKER_HALF);
  if (count > 1) {
    m5_pocket_bridge_set_number("imuMarker1X", cx[1] - MARKER_HALF);
    m5_pocket_bridge_set_number("imuMarker1Y", cy[1] - MARKER_HALF);
  }
}

void imu_svc_frame(void) {
  if (!atomic_load(&s_open)) {
    return;
  }
  if (atomic_load(&s_render_state) == RENDER_DONE) {
    svc_texture(TEXTURE, s_pixels, SIDE, SIDE, "imuTexture");
    if (s_state != READY) {
      s_state = READY;
      set_state(READY);
    }
    atomic_store(&s_render_state, RENDER_IDLE);
  }
  portENTER_CRITICAL(&s_lock);
  const bool ready = s_pose_ready;
  const uint32_t sequence = s_pose_sequence;
  const imu_quat_t relative = s_relative;
  portEXIT_CRITICAL(&s_lock);
  if (!ready) {
    imu_gesture_reset(&s_gesture);
    hide_markers();
    return;
  }
  update_gesture();
  const int64_t now = esp_timer_get_time();
  if ((sequence != s_drawn_pose || s_view_sequence != s_drawn_view) &&
      atomic_load(&s_render_state) == RENDER_IDLE && s_render &&
      now - s_render_requested_us >= RENDER_PERIOD_US) {
    s_render_requested_us = now;
    s_drawn_pose = sequence;
    s_drawn_view = s_view_sequence;
    s_render_pose = imu_quat_normalize(imu_quat_multiply(s_view, relative));
    atomic_store(&s_render_state, RENDER_BUSY);
    xTaskNotifyGive(s_render);
  }
}

static void open_window(void) {
  if (!s_pixels) {
    s_pixels = heap_caps_calloc(SIDE * SIDE, sizeof(uint16_t),
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  const unsigned generation = atomic_fetch_add(&s_generation, 1) + 1;
  portENTER_CRITICAL(&s_lock);
  s_pose_ready = false;
  s_relative = imu_quat_identity();
  portEXIT_CRITICAL(&s_lock);
  s_view = imu_quat_identity();
  s_view_sequence = 0;
  s_drawn_pose = UINT32_MAX;
  s_drawn_view = UINT32_MAX;
  imu_gesture_reset(&s_gesture);
  atomic_store(&s_recenter, false);
  s_state = STARTING;
  set_state(STARTING);
  m5_pocket_bridge_set_text("imuError", "");
  hide_markers();
  if (!s_pixels) {
    m5_pocket_bridge_set_text("imuError", "ESP_ERR_NO_MEM");
    set_state(FAILED);
    return;
  }
  atomic_store(&s_open, true);
  m5_corep4x_set_switch(M5_COREP4X_SWITCH_L3B, true);
  bool idle = false;
  if (m5_corep4x_imu_ready()) {
    start_fusion(generation);
  } else if (atomic_compare_exchange_strong(&s_init_running, &idle, true) &&
             xTaskCreate(init_task, "func_imu_init", 4096, NULL, 4, NULL) !=
                 pdPASS) {
    atomic_store(&s_init_running, false);
    m5_pocket_bridge_set_text("imuError", "ESP_ERR_NO_MEM");
    set_state(FAILED);
  }
}

static void close_window(void) {
  atomic_store(&s_open, false);
  atomic_fetch_add(&s_generation, 1);
  if (s_fusion) {
    xTaskNotifyGive(s_fusion);
  }
  hide_markers();
}

bool imu_svc_request(const m5_pocket_request *request) {
  if (svc_is(request, "imu")) {
    if (svc_arg(request, 0)) {
      open_window();
    } else {
      close_window();
    }
  } else if (svc_is(request, "imuRecenter")) {
    /* recenter_event_cb: the view offset back to identity, the fusion task
     * re-bootstraps from gravity at its next sample. */
    s_view = imu_quat_identity();
    s_view_sequence++;
    imu_gesture_reset(&s_gesture);
    atomic_store(&s_recenter, true);
  } else {
    return false;
  }
  return true;
}

void imu_svc_sleep(void) {
  close_window();
  for (int i = 0; i < 50 && s_fusion; i++) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void imu_svc_start(void) {
  set_state(STARTING);
  /* Core 1: the app task runs on core 0. */
  if (xTaskCreatePinnedToCore(render_task, "imu_render", 4096 + 8192, NULL, 3,
                              &s_render, 1) != pdPASS) {
    s_render = NULL;
    ESP_LOGE(TAG, "no render task");
  }
}
