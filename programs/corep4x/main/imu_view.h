/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * IMU window of the CoreP4X factory demo: attitude filter, software renderer
 * of the device model and touch gestures. Ported to C from
 * ui_function_imu_attitude.cpp, ui_function_imu_renderer.cpp and
 * update_touch_gesture of ui_function_imu_window.cpp in M5CoreP4X-UserDemo
 * commit cd1e1bd0bfd4b6e51a4898999b33a9392ea11f65.
 *
 * The factory shows normalize(view_offset * conj(reference) * orientation),
 * where reference is the filter orientation taken when calibration completes
 * or when the view is recentred, and view_offset is the gesture rotation.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The factory canvas, in pixels. Gesture points outside it end the gesture. */
#define IMU_VIEW_WIDTH 460
#define IMU_VIEW_HEIGHT 418

/* Texture sizes in pixels: front and back faces, then the four sides. */
#define IMU_TEXTURE_FACE_SIZE 256
#define IMU_TEXTURE_SIDE_WIDTH 256
#define IMU_TEXTURE_SIDE_HEIGHT 112

/* Unit quaternion that maps model vectors to the world frame. */
typedef struct {
  float w, x, y, z;
} imu_quat_t;

imu_quat_t imu_quat_identity(void);
imu_quat_t imu_quat_multiply(imu_quat_t l, imu_quat_t r);
imu_quat_t imu_quat_conjugate(imu_quat_t q);
/* A quaternion whose norm is below 1e-5 normalises to the identity. */
imu_quat_t imu_quat_normalize(imu_quat_t q);
/* ZYX order: qz(yaw) * qy(pitch) * qx(roll), angles in radians. */
imu_quat_t imu_quat_from_euler(float roll, float pitch, float yaw);

/* Mahony filter state, the members of CoreP4ImuAttitude. */
typedef struct {
  imu_quat_t orientation;
  float integral_error[3];
  float gyro_bias_dps[3];
  float gravity_norm;
  unsigned gyro_bias_samples;
  bool ready;
} imu_attitude_t;

void imu_attitude_reset(imu_attitude_t *a);
/*
 * One sample in the model frame (+X right, +Y up, +Z toward the viewer):
 * acceleration in g, angular rate in degrees per second. The first valid
 * acceleration sets the orientation from gravity. Returns false until then,
 * true afterwards. The gyro bias is the running mean of the first 60
 * consecutive still samples, then follows the input slowly while the board
 * stays still.
 */
bool imu_attitude_update(imu_attitude_t *a, float ax, float ay, float az,
                         bool acc_valid, float gx, float gy, float gz,
                         bool gyr_valid, float dt_seconds);
bool imu_attitude_calibrated(const imu_attitude_t *a);
/* Sets the orientation from gravity alone (yaw 0) and clears the integral
 * error; the gyro bias is kept. Returns false for a zero vector. */
bool imu_attitude_recenter(imu_attitude_t *a, float ax, float ay, float az);
void imu_attitude_clear_correction(imu_attitude_t *a);
imu_quat_t imu_attitude_orientation(const imu_attitude_t *a);

/*
 * Face textures as stored in the corep4x_*.rgb565 files: row-major RGB565
 * words with red in bits 11-15, read in place (the files are little-endian,
 * like the ESP32-P4). front and back are IMU_TEXTURE_FACE_SIZE square, the
 * sides IMU_TEXTURE_SIDE_WIDTH x IMU_TEXTURE_SIDE_HEIGHT. Each pointer must
 * be 2-byte aligned. A NULL face is drawn in the factory's bevel colour.
 */
typedef struct {
  const uint16_t *front, *back, *top, *bottom, *left, *right;
} imu_textures_t;

/*
 * Draws the device model at `pose` into `pixels` (width x height, `stride`
 * pixels per row) as the factory corep4_imu_render_device does: gradient
 * background, static ellipse shadow, 80 triangles with back-face culling,
 * flat shading, painter's order and perspective-correct nearest texture
 * lookup. Pixels are PSM_5650 words: red in bits 0-4, green in bits 5-10,
 * blue in bits 11-15; each colour is the factory's RGB565 colour with the red
 * and blue fields exchanged. Texels are converted while they are shaded, so
 * the textures are used as stored. No heap allocation; about 7 KB of stack
 * (80 triangle records).
 */
void imu_render_device(uint16_t *pixels, size_t stride, int width, int height,
                       imu_quat_t pose, const imu_textures_t *textures);

typedef enum {
  IMU_GESTURE_NONE,
  IMU_GESTURE_DRAG,
  IMU_GESTURE_TWIST,
} imu_gesture_mode_t;

/* Touch gesture state of the IMU window. */
typedef struct {
  imu_gesture_mode_t mode;
  int32_t last_x;
  int32_t last_y;
  float last_angle;
} imu_gesture_t;

/* Ends any gesture; the factory does this when the view is recentred and
 * while no pose has been published. */
void imu_gesture_reset(imu_gesture_t *g);
/*
 * One touch sample, called once per view update (25 ms in the factory) with
 * `count` points (0 to 2) in canvas pixels. One finger drags: 0.006 rad per
 * pixel about the screen axes. Two fingers twist the model about the view
 * axis with the line between them. A change of finger count starts the new
 * gesture without moving the model. Returns true when `view_offset` changed.
 */
bool imu_gesture_update(imu_gesture_t *g, int count, const int16_t *xs,
                        const int16_t *ys, imu_quat_t *view_offset);
