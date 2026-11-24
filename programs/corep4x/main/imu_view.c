/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * C port of ui_function_imu_attitude.cpp (Mahony attitude filter),
 * ui_function_imu_renderer.cpp (software renderer of the device model) and
 * update_touch_gesture of ui_function_imu_window.cpp (drag and twist
 * gestures) from M5CoreP4X-UserDemo commit
 * cd1e1bd0bfd4b6e51a4898999b33a9392ea11f65. Constants, formulas and the order
 * of the floating-point operations follow the C++, so the filter and the
 * frames give the factory values. The renderer writes PSM_5650 words (red in
 * the low bits) where the factory writes RGB565 (red in the high bits).
 */
#include "imu_view.h"

#include <math.h>

#define IMU_PI 3.14159265358979323846f

/* std::min and std::max, including which operand wins a comparison. */
static float min_f(float a, float b) { return b < a ? b : a; }

static float max_f(float a, float b) { return a < b ? b : a; }

static float clamp_f(float value, float minimum, float maximum) {
  return max_f(minimum, min_f(value, maximum));
}

static int min_i(int a, int b) { return b < a ? b : a; }

static int max_i(int a, int b) { return a < b ? b : a; }

/* Quaternions ------------------------------------------------------------ */

#define MIN_VECTOR_NORM 1.0e-5f

imu_quat_t imu_quat_identity(void) {
  const imu_quat_t identity = {1.0f, 0.0f, 0.0f, 0.0f};
  return identity;
}

imu_quat_t imu_quat_normalize(imu_quat_t q) {
  const float norm = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  if (norm < MIN_VECTOR_NORM) {
    return imu_quat_identity();
  }
  const float inverse = 1.0f / norm;
  q.w *= inverse;
  q.x *= inverse;
  q.y *= inverse;
  q.z *= inverse;
  return q;
}

imu_quat_t imu_quat_conjugate(imu_quat_t q) {
  q.x = -q.x;
  q.y = -q.y;
  q.z = -q.z;
  return q;
}

imu_quat_t imu_quat_multiply(imu_quat_t l, imu_quat_t r) {
  const imu_quat_t product = {
      l.w * r.w - l.x * r.x - l.y * r.y - l.z * r.z,
      l.w * r.x + l.x * r.w + l.y * r.z - l.z * r.y,
      l.w * r.y - l.x * r.z + l.y * r.w + l.z * r.x,
      l.w * r.z + l.x * r.y - l.y * r.x + l.z * r.w,
  };
  return product;
}

imu_quat_t imu_quat_from_euler(float roll, float pitch, float yaw) {
  const float cr = cosf(roll * 0.5f);
  const float sr = sinf(roll * 0.5f);
  const float cp = cosf(pitch * 0.5f);
  const float sp = sinf(pitch * 0.5f);
  const float cy = cosf(yaw * 0.5f);
  const float sy = sinf(yaw * 0.5f);
  const imu_quat_t q = {
      cr * cp * cy + sr * sp * sy,
      sr * cp * cy - cr * sp * sy,
      cr * sp * cy + sr * cp * sy,
      cr * cp * sy - sr * sp * cy,
  };
  return imu_quat_normalize(q);
}

/* Attitude filter -------------------------------------------------------- */

#define DEG_TO_RAD (IMU_PI / 180.0f)
#define MAHONY_KP 2.8f
#define MAHONY_KI 0.06f
#define MAX_DELTA_SECONDS 0.1f
#define MIN_DELTA_SECONDS 0.001f
#define BOOTSTRAP_STATIONARY_GYRO_DPS 5.0f
#define TRACKING_STATIONARY_GYRO_DPS 0.25f
#define STATIONARY_ACCEL_FRACTION 0.10f
/* 600 ms of still samples at the factory's 10 ms sample period. */
#define GYRO_BIAS_BOOTSTRAP_SAMPLES 60U
#define GYRO_BIAS_ALPHA 0.003f
#define INTEGRAL_ERROR_LIMIT 0.08f
#define INTEGRAL_ERROR_DECAY 0.98f
/* Low-pass weight of the gravity magnitude, and the band around it in which
 * the accelerometer corrects the attitude. */
#define GRAVITY_NORM_WEIGHT 0.02f
#define CORRECTION_ACCEL_FRACTION 0.20f

void imu_attitude_clear_correction(imu_attitude_t *a) {
  a->integral_error[0] = 0.0f;
  a->integral_error[1] = 0.0f;
  a->integral_error[2] = 0.0f;
}

void imu_attitude_reset(imu_attitude_t *a) {
  a->orientation = imu_quat_identity();
  imu_attitude_clear_correction(a);
  a->gyro_bias_dps[0] = 0.0f;
  a->gyro_bias_dps[1] = 0.0f;
  a->gyro_bias_dps[2] = 0.0f;
  a->gravity_norm = 0.0f;
  a->gyro_bias_samples = 0;
  a->ready = false;
}

bool imu_attitude_recenter(imu_attitude_t *a, float ax, float ay, float az) {
  const float norm = sqrtf(ax * ax + ay * ay + az * az);
  if (norm < MIN_VECTOR_NORM) {
    return false;
  }
  const float roll = atan2f(ay, az);
  const float pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
  a->orientation = imu_quat_from_euler(roll, pitch, 0.0f);
  a->gravity_norm = norm;
  imu_attitude_clear_correction(a);
  a->ready = true;
  return true;
}

bool imu_attitude_calibrated(const imu_attitude_t *a) {
  return a->gyro_bias_samples >= GYRO_BIAS_BOOTSTRAP_SAMPLES;
}

imu_quat_t imu_attitude_orientation(const imu_attitude_t *a) {
  return a->orientation;
}

bool imu_attitude_update(imu_attitude_t *a, float ax, float ay, float az,
                         bool acc_valid, float gx, float gy, float gz,
                         bool gyr_valid, float dt_seconds) {
  const float acc_norm = acc_valid ? sqrtf(ax * ax + ay * ay + az * az) : 0.0f;
  acc_valid = acc_valid && acc_norm > MIN_VECTOR_NORM;

  if (!a->ready) {
    if (!acc_valid || !imu_attitude_recenter(a, ax, ay, az)) {
      return false;
    }
  }

  if (!gyr_valid) {
    return a->ready;
  }

  const float dt = clamp_f(dt_seconds, MIN_DELTA_SECONDS, MAX_DELTA_SECONDS);
  if (acc_valid) {
    a->gravity_norm += (acc_norm - a->gravity_norm) * GRAVITY_NORM_WEIGHT;
  }

  float *bias = a->gyro_bias_dps;
  const float raw_gyro_norm = sqrtf(gx * gx + gy * gy + gz * gz);
  const float corrected_x = gx - bias[0];
  const float corrected_y = gy - bias[1];
  const float corrected_z = gz - bias[2];
  const float corrected_gyro_norm =
      sqrtf(corrected_x * corrected_x + corrected_y * corrected_y +
            corrected_z * corrected_z);
  const bool acceleration_stationary =
      acc_valid &&
      fabsf(acc_norm - a->gravity_norm) <=
          max_f(a->gravity_norm * STATIONARY_ACCEL_FRACTION, 0.01f);
  const bool bootstrap_stationary =
      acceleration_stationary && raw_gyro_norm < BOOTSTRAP_STATIONARY_GYRO_DPS;
  const bool tracking_stationary =
      acceleration_stationary &&
      corrected_gyro_norm < TRACKING_STATIONARY_GYRO_DPS;
  if (!imu_attitude_calibrated(a)) {
    /* Running mean of consecutive still samples; motion restarts it. */
    if (bootstrap_stationary) {
      ++a->gyro_bias_samples;
      const float sample_weight = 1.0f / (float)a->gyro_bias_samples;
      bias[0] += (gx - bias[0]) * sample_weight;
      bias[1] += (gy - bias[1]) * sample_weight;
      bias[2] += (gz - bias[2]) * sample_weight;
    } else {
      a->gyro_bias_samples = 0;
      bias[0] = 0.0f;
      bias[1] = 0.0f;
      bias[2] = 0.0f;
    }
  } else if (tracking_stationary) {
    bias[0] += (gx - bias[0]) * GYRO_BIAS_ALPHA;
    bias[1] += (gy - bias[1]) * GYRO_BIAS_ALPHA;
    bias[2] += (gz - bias[2]) * GYRO_BIAS_ALPHA;
  }

  float rate_x = (gx - bias[0]) * DEG_TO_RAD;
  float rate_y = (gy - bias[1]) * DEG_TO_RAD;
  float rate_z = (gz - bias[2]) * DEG_TO_RAD;

  float *integral = a->integral_error;
  if (acc_valid && fabsf(acc_norm - a->gravity_norm) <=
                       a->gravity_norm * CORRECTION_ACCEL_FRACTION) {
    const float inverse_acc = 1.0f / acc_norm;
    const float nx = ax * inverse_acc;
    const float ny = ay * inverse_acc;
    const float nz = az * inverse_acc;
    const imu_quat_t q = a->orientation;

    /* World +Z (up) in the model frame, against the measured gravity. */
    const float vx = 2.0f * (q.x * q.z - q.w * q.y);
    const float vy = 2.0f * (q.w * q.x + q.y * q.z);
    const float vz = q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z;
    const float error_x = ny * vz - nz * vy;
    const float error_y = nz * vx - nx * vz;
    const float error_z = nx * vy - ny * vx;

    integral[0] = clamp_f(integral[0] + MAHONY_KI * error_x * dt,
                          -INTEGRAL_ERROR_LIMIT, INTEGRAL_ERROR_LIMIT);
    integral[1] = clamp_f(integral[1] + MAHONY_KI * error_y * dt,
                          -INTEGRAL_ERROR_LIMIT, INTEGRAL_ERROR_LIMIT);
    integral[2] = clamp_f(integral[2] + MAHONY_KI * error_z * dt,
                          -INTEGRAL_ERROR_LIMIT, INTEGRAL_ERROR_LIMIT);
    rate_x += MAHONY_KP * error_x + integral[0];
    rate_y += MAHONY_KP * error_y + integral[1];
    rate_z += MAHONY_KP * error_z + integral[2];
  } else {
    integral[0] *= INTEGRAL_ERROR_DECAY;
    integral[1] *= INTEGRAL_ERROR_DECAY;
    integral[2] *= INTEGRAL_ERROR_DECAY;
  }

  /* First-order integration of q' = q * (0, rate) / 2. */
  const imu_quat_t q = a->orientation;
  const float half_dt = 0.5f * dt;
  imu_quat_t next = q;
  next.w += (-q.x * rate_x - q.y * rate_y - q.z * rate_z) * half_dt;
  next.x += (q.w * rate_x + q.y * rate_z - q.z * rate_y) * half_dt;
  next.y += (q.w * rate_y - q.x * rate_z + q.z * rate_x) * half_dt;
  next.z += (q.w * rate_z + q.x * rate_y - q.y * rate_x) * half_dt;
  a->orientation = imu_quat_normalize(next);
  return true;
}

/* Renderer --------------------------------------------------------------- */

/*
 * Model frame: +X right, +Y up, +Z toward the camera, which sits on the +Z
 * axis. The housing is a 54 x 54 rounded square, 23.6 deep, with 4 segments
 * per rounded corner: 20 perimeter points, 20 front and 20 back fan
 * triangles, 40 side triangles.
 */
#define DEVICE_HALF_WIDTH 27.0f
#define DEVICE_HALF_DEPTH 11.8f
#define DEVICE_CORNER_RADIUS 5.2f
#define CORNER_SEGMENTS 4
#define PERIMETER_POINTS (4 * (CORNER_SEGMENTS + 1))
#define MAX_RENDER_TRIANGLES (PERIMETER_POINTS * 4)
#define CAMERA_DISTANCE 108.0f
#define FOCAL_LENGTH 575.0f
#define MIN_VERTEX_DEPTH 8.0f
/* Colour of a face without a texture, in RGB565 (it reads the same in
 * PSM_5650: the red and blue fields hold the same value). */
#define BEVEL_COLOR 0xBDF7U

typedef struct {
  float x, y, z;
} vec3_t;

typedef struct {
  const uint16_t *data;
  int width;
  int height;
} texture_t;

typedef enum {
  SIDE_TOP,
  SIDE_BOTTOM,
  SIDE_LEFT,
  SIDE_RIGHT,
} side_face_t;

enum {
  FACE_FRONT,
  FACE_BACK,
  FACE_TOP,
  FACE_BOTTOM,
  FACE_LEFT,
  FACE_RIGHT,
  FACE_COUNT,
};

typedef struct {
  vec3_t position;
  float u, v;
} model_vertex_t;

/* Screen position plus 1/depth, u/depth and v/depth, which are affine in
 * screen space. */
typedef struct {
  float x, y;
  float inverse_depth;
  float u_over_depth;
  float v_over_depth;
} screen_vertex_t;

typedef struct {
  screen_vertex_t vertices[3];
  const texture_t *texture; /* NULL: BEVEL_COLOR */
  float shade;
  float depth;
} render_triangle_t;

typedef struct {
  render_triangle_t *triangles;
  int count;
  int width;
  int height;
  imu_quat_t rotation;
  vec3_t light;
} triangle_builder_t;

static vec3_t vec3(float x, float y, float z) {
  const vec3_t v = {x, y, z};
  return v;
}

static vec3_t subtract(vec3_t l, vec3_t r) {
  return vec3(l.x - r.x, l.y - r.y, l.z - r.z);
}

static vec3_t cross(vec3_t l, vec3_t r) {
  return vec3(l.y * r.z - l.z * r.y, l.z * r.x - l.x * r.z,
              l.x * r.y - l.y * r.x);
}

static float dot(vec3_t l, vec3_t r) {
  return l.x * r.x + l.y * r.y + l.z * r.z;
}

static vec3_t normalized(vec3_t v) {
  const float length = sqrtf(dot(v, v));
  if (length < 1.0e-6f) {
    return vec3(0.0f, 0.0f, 1.0f);
  }
  const float inverse = 1.0f / length;
  return vec3(v.x * inverse, v.y * inverse, v.z * inverse);
}

/* v + 2 * (w * (q x v) + q x (q x v)), q the vector part. */
static vec3_t rotate(imu_quat_t q, vec3_t v) {
  const vec3_t qv = vec3(q.x, q.y, q.z);
  const vec3_t first = cross(qv, v);
  const vec3_t second = cross(qv, first);
  return vec3(v.x + 2.0f * (q.w * first.x + second.x),
              v.y + 2.0f * (q.w * first.y + second.y),
              v.z + 2.0f * (q.w * first.z + second.z));
}

/* The factory's rgb565(r, g, b) with the red and blue fields exchanged. */
static uint16_t psm5650(uint8_t red, uint8_t green, uint8_t blue) {
  return (uint16_t)(((blue & 0xF8U) << 8U) | ((green & 0xFCU) << 3U) |
                    (red >> 3U));
}

/*
 * The factory's mix_rgb565. It works on the 5-bit field at bit 11, the 6-bit
 * field at bit 5 and the 5-bit field at bit 0 with the same arithmetic for
 * the two 5-bit fields, so on PSM_5650 words it gives the factory result with
 * red and blue exchanged.
 */
static uint16_t mix_565(uint16_t source, uint16_t target, float amount) {
  amount = max_f(0.0f, min_f(amount, 1.0f));
  const int source_hi = (source >> 11) & 0x1F;
  const int source_g = (source >> 5) & 0x3F;
  const int source_lo = source & 0x1F;
  const int target_hi = (target >> 11) & 0x1F;
  const int target_g = (target >> 5) & 0x3F;
  const int target_lo = target & 0x1F;
  const int hi =
      (int)((float)source_hi + (float)(target_hi - source_hi) * amount);
  const int green =
      (int)((float)source_g + (float)(target_g - source_g) * amount);
  const int lo =
      (int)((float)source_lo + (float)(target_lo - source_lo) * amount);
  return (uint16_t)((hi << 11) | (green << 5) | lo);
}

/*
 * The factory's shade_rgb565 on an RGB565 texel, with the result packed as
 * PSM_5650. `shade` is already clamped to [0, 1].
 */
static uint16_t shade_to_psm5650(uint16_t color, float shade) {
  const int red = (int)((float)((color >> 11) & 0x1F) * shade);
  const int green = (int)((float)((color >> 5) & 0x3F) * shade);
  const int blue = (int)((float)(color & 0x1F) * shade);
  return (uint16_t)((blue << 11) | (green << 5) | red);
}

static void fill_background(uint16_t *pixels, size_t stride, int width,
                            int height) {
  /* RGB565 0xFFDF, 0xE75D and 0x424A in the factory. */
  const uint16_t top = psm5650(248, 250, 252);
  const uint16_t bottom = psm5650(224, 232, 236);
  for (int y = 0; y < height; ++y) {
    const float amount = height > 1 ? (float)y / (float)(height - 1) : 0.0f;
    const uint16_t color = mix_565(top, bottom, amount);
    uint16_t *row = pixels + (size_t)y * stride;
    for (int x = 0; x < width; ++x) {
      row[x] = color;
    }
  }

  /* A fixed shadow under the model; it does not follow the pose. Pixel
   * positions are the integer coordinates, without the 0.5 centre offset. */
  const float center_x = (float)width * 0.50f;
  const float center_y = (float)height * 0.73f;
  const float radius_x = (float)width * 0.31f;
  const float radius_y = (float)height * 0.085f;
  const uint16_t shadow = psm5650(64, 75, 82);
  const int x0 = max_i(0, (int)(center_x - radius_x));
  const int x1 = min_i(width - 1, (int)(center_x + radius_x));
  const int y0 = max_i(0, (int)(center_y - radius_y));
  const int y1 = min_i(height - 1, (int)(center_y + radius_y));
  for (int y = y0; y <= y1; ++y) {
    uint16_t *row = pixels + (size_t)y * stride;
    for (int x = x0; x <= x1; ++x) {
      const float dx = ((float)x - center_x) / radius_x;
      const float dy = ((float)y - center_y) / radius_y;
      const float distance = dx * dx + dy * dy;
      if (distance >= 1.0f) {
        continue;
      }
      const float alpha = (1.0f - distance) * (1.0f - distance) * 0.18f;
      row[x] = mix_565(row[x], shadow, alpha);
    }
  }
}

static float edge(const screen_vertex_t *a, const screen_vertex_t *b, float x,
                  float y) {
  return (x - a->x) * (b->y - a->y) - (y - a->y) * (b->x - a->x);
}

/* Nearest texel; u and v are clamped to [0, 0.99999]. */
static uint16_t sample_texture(const texture_t *texture, float u, float v) {
  u = max_f(0.0f, min_f(u, 0.99999f));
  v = max_f(0.0f, min_f(v, 0.99999f));
  const int x = (int)(u * (float)texture->width);
  const int y = (int)(v * (float)texture->height);
  return texture->data[(size_t)(y * texture->width + x)];
}

/*
 * Pixel centres at +0.5; a pixel is inside when no barycentric weight is
 * below -0.0001, so pixels on a shared edge are drawn by both triangles.
 */
static void rasterize_triangle(uint16_t *pixels, size_t stride, int width,
                               int height, const render_triangle_t *triangle) {
  const screen_vertex_t *a = &triangle->vertices[0];
  const screen_vertex_t *b = &triangle->vertices[1];
  const screen_vertex_t *c = &triangle->vertices[2];
  const float area = edge(a, b, c->x, c->y);
  if (fabsf(area) < 0.01f) {
    return;
  }

  const int min_x = max_i(0, (int)floorf(min_f(a->x, min_f(b->x, c->x))));
  const int max_x =
      min_i(width - 1, (int)ceilf(max_f(a->x, max_f(b->x, c->x))));
  const int min_y = max_i(0, (int)floorf(min_f(a->y, min_f(b->y, c->y))));
  const int max_y =
      min_i(height - 1, (int)ceilf(max_f(a->y, max_f(b->y, c->y))));

  const texture_t *texture = triangle->texture;
  const float shade = triangle->shade;
  for (int y = min_y; y <= max_y; ++y) {
    uint16_t *row = pixels + (size_t)y * stride;
    const float py = (float)y + 0.5f;
    for (int x = min_x; x <= max_x; ++x) {
      const float px = (float)x + 0.5f;
      const float weight_a = edge(b, c, px, py) / area;
      const float weight_b = edge(c, a, px, py) / area;
      const float weight_c = 1.0f - weight_a - weight_b;
      if (weight_a < -0.0001f || weight_b < -0.0001f || weight_c < -0.0001f) {
        continue;
      }

      uint16_t color = (uint16_t)BEVEL_COLOR;
      if (texture != NULL) {
        const float inverse_depth = weight_a * a->inverse_depth +
                                    weight_b * b->inverse_depth +
                                    weight_c * c->inverse_depth;
        if (inverse_depth <= 0.0f) {
          continue;
        }
        const float u =
            (weight_a * a->u_over_depth + weight_b * b->u_over_depth +
             weight_c * c->u_over_depth) /
            inverse_depth;
        const float v =
            (weight_a * a->v_over_depth + weight_b * b->v_over_depth +
             weight_c * c->v_over_depth) /
            inverse_depth;
        color = sample_texture(texture, u, v);
      }
      row[x] = shade_to_psm5650(color, shade);
    }
  }
}

/* Rotates, culls, lights and projects one triangle. */
static void builder_add(triangle_builder_t *builder, const model_vertex_t *a,
                        const model_vertex_t *b, const model_vertex_t *c,
                        const texture_t *texture) {
  if (builder->count >= MAX_RENDER_TRIANGLES) {
    return;
  }
  const model_vertex_t *input[3] = {a, b, c};
  vec3_t positions[3];
  for (int i = 0; i < 3; ++i) {
    positions[i] = rotate(builder->rotation, input[i]->position);
  }
  const vec3_t normal_raw = cross(subtract(positions[1], positions[0]),
                                  subtract(positions[2], positions[0]));
  const vec3_t center =
      vec3((positions[0].x + positions[1].x + positions[2].x) / 3.0f,
           (positions[0].y + positions[1].y + positions[2].y) / 3.0f,
           (positions[0].z + positions[1].z + positions[2].z) / 3.0f);
  /* Back-face cull against the direction from the centroid to the camera. */
  if (dot(normal_raw, vec3(-center.x, -center.y, CAMERA_DISTANCE - center.z)) <=
      0.0f) {
    return;
  }

  const vec3_t normal = normalized(normal_raw);
  render_triangle_t *output = &builder->triangles[builder->count++];
  output->texture = texture != NULL && texture->data != NULL ? texture : NULL;
  /* shade_rgb565 clamps the shade to [0, 1] for every pixel; it is clamped
   * once here. */
  output->shade = clamp_f(
      0.58f + 0.42f * max_f(0.0f, dot(normal, builder->light)), 0.0f, 1.0f);
  output->depth = center.z;

  for (int i = 0; i < 3; ++i) {
    const float depth =
        max_f(MIN_VERTEX_DEPTH, CAMERA_DISTANCE - positions[i].z);
    const float inverse_depth = 1.0f / depth;
    screen_vertex_t *vertex = &output->vertices[i];
    vertex->x = (float)builder->width * 0.5f +
                FOCAL_LENGTH * positions[i].x * inverse_depth;
    vertex->y = (float)builder->height * 0.48f -
                FOCAL_LENGTH * positions[i].y * inverse_depth;
    vertex->inverse_depth = inverse_depth;
    vertex->u_over_depth = input[i]->u * inverse_depth;
    vertex->v_over_depth = input[i]->v * inverse_depth;
  }
}

/*
 * Counter-clockwise outline seen from +Z, starting at (27, 21.8): corner 0
 * top right, 1 top left, 2 bottom left, 3 bottom right, 5 points per corner.
 */
static void rounded_perimeter(float z, vec3_t points[PERIMETER_POINTS]) {
  int index = 0;
  for (int corner = 0; corner < 4; ++corner) {
    const float center_x = (corner == 0 || corner == 3)
                               ? DEVICE_HALF_WIDTH - DEVICE_CORNER_RADIUS
                               : -DEVICE_HALF_WIDTH + DEVICE_CORNER_RADIUS;
    const float center_y = (corner == 0 || corner == 1)
                               ? DEVICE_HALF_WIDTH - DEVICE_CORNER_RADIUS
                               : -DEVICE_HALF_WIDTH + DEVICE_CORNER_RADIUS;
    const float start_angle = (float)corner * 90.0f;
    for (int segment = 0; segment <= CORNER_SEGMENTS; ++segment) {
      const float angle =
          (start_angle + 90.0f * (float)segment / (float)CORNER_SEGMENTS) *
          IMU_PI / 180.0f;
      points[index++] = vec3(center_x + DEVICE_CORNER_RADIUS * cosf(angle),
                             center_y + DEVICE_CORNER_RADIUS * sinf(angle), z);
    }
  }
}

/*
 * u along a side face from the arc length of the vertex on the front ring.
 * Each side starts at the 45 degree point of a corner and is a quarter of the
 * perimeter long: top and bottom run 0 at -X to 1 at +X, left and right run
 * 0 at +Y to 1 at -Y. v runs 0 at the front edge to 1 at the back edge.
 */
static float side_texture_u(side_face_t face, float perimeter_distance,
                            float side_start, float side_length,
                            float perimeter_length) {
  switch (face) {
  case SIDE_TOP:
    return (side_start + side_length - perimeter_distance) / side_length;
  case SIDE_LEFT:
    return (perimeter_distance - side_start - side_length) / side_length;
  case SIDE_BOTTOM:
    return (perimeter_distance - side_start - 2.0f * side_length) / side_length;
  case SIDE_RIGHT:
    if (perimeter_distance <= side_start) {
      perimeter_distance += perimeter_length;
    }
    return (side_start + 4.0f * side_length - perimeter_distance) / side_length;
  }
  return 0.0f;
}

static model_vertex_t model_vertex(vec3_t position, float u, float v) {
  const model_vertex_t vertex = {position, u, v};
  return vertex;
}

/* Front ring texture coordinates: the image seen from +Z. */
static model_vertex_t front_vertex(vec3_t point) {
  return model_vertex(
      point, (point.x + DEVICE_HALF_WIDTH) / (2.0f * DEVICE_HALF_WIDTH),
      (DEVICE_HALF_WIDTH - point.y) / (2.0f * DEVICE_HALF_WIDTH));
}

/* Back ring texture coordinates, mirrored in x: the image seen from -Z. */
static model_vertex_t back_vertex(vec3_t point) {
  return model_vertex(
      point, (DEVICE_HALF_WIDTH - point.x) / (2.0f * DEVICE_HALF_WIDTH),
      (DEVICE_HALF_WIDTH - point.y) / (2.0f * DEVICE_HALF_WIDTH));
}

static void add_device_mesh(triangle_builder_t *builder,
                            const texture_t faces[FACE_COUNT]) {
  vec3_t front[PERIMETER_POINTS];
  vec3_t back[PERIMETER_POINTS];
  rounded_perimeter(DEVICE_HALF_DEPTH, front);
  rounded_perimeter(-DEVICE_HALF_DEPTH, back);

  float perimeter_distances[PERIMETER_POINTS];
  perimeter_distances[0] = 0.0f;
  for (int i = 1; i < PERIMETER_POINTS; ++i) {
    const vec3_t segment = subtract(front[i], front[i - 1]);
    perimeter_distances[i] =
        perimeter_distances[i - 1] + sqrtf(dot(segment, segment));
  }
  const vec3_t closing_segment =
      subtract(front[0], front[PERIMETER_POINTS - 1]);
  const float perimeter_length = perimeter_distances[PERIMETER_POINTS - 1] +
                                 sqrtf(dot(closing_segment, closing_segment));
  const float side_start = perimeter_distances[CORNER_SEGMENTS / 2];
  const float side_length = perimeter_length * 0.25f;
  const model_vertex_t front_center =
      model_vertex(vec3(0.0f, 0.0f, DEVICE_HALF_DEPTH), 0.5f, 0.5f);
  const model_vertex_t back_center =
      model_vertex(vec3(0.0f, 0.0f, -DEVICE_HALF_DEPTH), 0.5f, 0.5f);

  for (int i = 0; i < PERIMETER_POINTS; ++i) {
    const int next = (i + 1) % PERIMETER_POINTS;
    const model_vertex_t front_i = front_vertex(front[i]);
    const model_vertex_t front_next = front_vertex(front[next]);
    builder_add(builder, &front_center, &front_i, &front_next,
                &faces[FACE_FRONT]);
    /* Reversed winding: the back fan faces -Z. */
    const model_vertex_t back_next = back_vertex(back[next]);
    const model_vertex_t back_i = back_vertex(back[i]);
    builder_add(builder, &back_center, &back_next, &back_i, &faces[FACE_BACK]);

    /* The side face is the one the 2D outward normal of the edge points to. */
    const float dx = front[next].x - front[i].x;
    const float dy = front[next].y - front[i].y;
    const float length = sqrtf(dx * dx + dy * dy);
    const float outward_x = length > 0.0f ? dy / length : 0.0f;
    const float outward_y = length > 0.0f ? -dx / length : 0.0f;
    side_face_t face;
    if (fabsf(outward_y) > fabsf(outward_x)) {
      face = outward_y > 0.0f ? SIDE_TOP : SIDE_BOTTOM;
    } else {
      face = outward_x > 0.0f ? SIDE_RIGHT : SIDE_LEFT;
    }
    const texture_t *texture = NULL;
    switch (face) {
    case SIDE_TOP:
      texture = &faces[FACE_TOP];
      break;
    case SIDE_BOTTOM:
      texture = &faces[FACE_BOTTOM];
      break;
    case SIDE_LEFT:
      texture = &faces[FACE_LEFT];
      break;
    case SIDE_RIGHT:
      texture = &faces[FACE_RIGHT];
      break;
    }
    const float distance_a = perimeter_distances[i];
    const float distance_b =
        next == 0 ? perimeter_length : perimeter_distances[next];
    const float u_a = side_texture_u(face, distance_a, side_start, side_length,
                                     perimeter_length);
    const float u_b = side_texture_u(face, distance_b, side_start, side_length,
                                     perimeter_length);

    const model_vertex_t a_front = model_vertex(front[i], u_a, 0.0f);
    const model_vertex_t a_back = model_vertex(back[i], u_a, 1.0f);
    const model_vertex_t b_back = model_vertex(back[next], u_b, 1.0f);
    const model_vertex_t b_front = model_vertex(front[next], u_b, 0.0f);
    builder_add(builder, &a_front, &a_back, &b_back, texture);
    builder_add(builder, &a_front, &b_back, &b_front, texture);
  }
}

static texture_t make_texture(const uint16_t *data, int width, int height) {
  const texture_t texture = {data, width, height};
  return texture;
}

void imu_render_device(uint16_t *pixels, size_t stride, int width, int height,
                       imu_quat_t pose, const imu_textures_t *textures) {
  if (pixels == NULL || width <= 0 || height <= 0 || stride < (size_t)width) {
    return;
  }

  fill_background(pixels, stride, width, height);

  texture_t faces[FACE_COUNT];
  const imu_textures_t none = {NULL, NULL, NULL, NULL, NULL, NULL};
  const imu_textures_t *t = textures != NULL ? textures : &none;
  faces[FACE_FRONT] =
      make_texture(t->front, IMU_TEXTURE_FACE_SIZE, IMU_TEXTURE_FACE_SIZE);
  faces[FACE_BACK] =
      make_texture(t->back, IMU_TEXTURE_FACE_SIZE, IMU_TEXTURE_FACE_SIZE);
  faces[FACE_TOP] =
      make_texture(t->top, IMU_TEXTURE_SIDE_WIDTH, IMU_TEXTURE_SIDE_HEIGHT);
  faces[FACE_BOTTOM] =
      make_texture(t->bottom, IMU_TEXTURE_SIDE_WIDTH, IMU_TEXTURE_SIDE_HEIGHT);
  faces[FACE_LEFT] =
      make_texture(t->left, IMU_TEXTURE_SIDE_WIDTH, IMU_TEXTURE_SIDE_HEIGHT);
  faces[FACE_RIGHT] =
      make_texture(t->right, IMU_TEXTURE_SIDE_WIDTH, IMU_TEXTURE_SIDE_HEIGHT);

  render_triangle_t triangles[MAX_RENDER_TRIANGLES];
  triangle_builder_t builder = {
      .triangles = triangles,
      .count = 0,
      .width = width,
      .height = height,
      .rotation = imu_quat_normalize(pose),
      .light = normalized(vec3(-0.35f, 0.55f, 0.76f)),
  };
  add_device_mesh(&builder, faces);

  /*
   * Painter's order: ascending centroid depth, so the farthest triangle is
   * drawn first. The factory uses std::sort, which leaves the order of equal
   * depths unspecified; this stable insertion sort keeps them in build
   * order. Triangles of equal depth that overlap share at most an edge.
   */
  uint8_t order[MAX_RENDER_TRIANGLES];
  for (int i = 0; i < builder.count; ++i) {
    int j = i;
    while (j > 0 && triangles[i].depth < triangles[order[j - 1]].depth) {
      order[j] = order[j - 1];
      --j;
    }
    order[j] = (uint8_t)i;
  }
  for (int i = 0; i < builder.count; ++i) {
    rasterize_triangle(pixels, stride, width, height, &triangles[order[i]]);
  }
}

/* Gestures --------------------------------------------------------------- */

#define TOUCH_DRAG_RADIANS_PER_PIXEL 0.006f
/* A larger twist step between two samples is taken as a finger swap or a
 * misread and is ignored. */
#define TWIST_MAX_STEP 0.35f

static bool point_in_view(int x, int y) {
  return x >= 0 && x < IMU_VIEW_WIDTH && y >= 0 && y < IMU_VIEW_HEIGHT;
}

/*
 * Folds an angle step into [-pi/2, pi/2]: the line through two fingers has
 * no direction, so a step of pi is no rotation. The C++ while loops leave
 * both -pi/2 and +pi/2 unchanged, so the interval is closed at both ends.
 */
static float normalize_twist_delta(float delta) {
  const float half_pi = IMU_PI * 0.5f;
  while (delta > half_pi) {
    delta -= IMU_PI;
  }
  while (delta < -half_pi) {
    delta += IMU_PI;
  }
  return delta;
}

/* Pre-multiplied: the step turns the model about the screen axes. */
static void apply_view_rotation(imu_quat_t *view_offset, imu_quat_t delta) {
  *view_offset = imu_quat_normalize(imu_quat_multiply(delta, *view_offset));
}

void imu_gesture_reset(imu_gesture_t *g) {
  g->mode = IMU_GESTURE_NONE;
  g->last_x = 0;
  g->last_y = 0;
  g->last_angle = 0.0f;
}

bool imu_gesture_update(imu_gesture_t *g, int count, const int16_t *xs,
                        const int16_t *ys, imu_quat_t *view_offset) {
  if (g == NULL || view_offset == NULL) {
    return false;
  }
  if (count > 2) {
    count = 2;
  }
  if (count <= 0 || xs == NULL || ys == NULL || !point_in_view(xs[0], ys[0]) ||
      (count > 1 && !point_in_view(xs[1], ys[1]))) {
    g->mode = IMU_GESTURE_NONE;
    return false;
  }

  bool changed = false;
  if (count == 1) {
    if (g->mode == IMU_GESTURE_DRAG) {
      const float dx = (float)(xs[0] - g->last_x);
      const float dy = (float)(ys[0] - g->last_y);
      if (dx != 0.0f || dy != 0.0f) {
        /* Vertical motion turns about X, horizontal motion about Y. */
        apply_view_rotation(
            view_offset,
            imu_quat_from_euler(dy * TOUCH_DRAG_RADIANS_PER_PIXEL,
                                dx * TOUCH_DRAG_RADIANS_PER_PIXEL, 0.0f));
        changed = true;
      }
    }
    g->mode = IMU_GESTURE_DRAG;
    g->last_x = xs[0];
    g->last_y = ys[0];
    return changed;
  }

  /* Screen y points down, so the model turns by -delta about +Z to follow
   * the fingers. */
  const float twist_angle =
      atan2f((float)(ys[1] - ys[0]), (float)(xs[1] - xs[0]));
  if (g->mode == IMU_GESTURE_TWIST) {
    const float delta = normalize_twist_delta(twist_angle - g->last_angle);
    if (fabsf(delta) < TWIST_MAX_STEP) {
      apply_view_rotation(view_offset, imu_quat_from_euler(0.0f, 0.0f, -delta));
      changed = true;
    }
  }
  g->mode = IMU_GESTURE_TWIST;
  g->last_angle = twist_angle;
  return changed;
}
