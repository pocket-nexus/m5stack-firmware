// SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
// SPDX-License-Identifier: MIT
//
// The internal I2C bus and the IMU of either board. The AtomS3R's BMI270
// samples are turned into M5Unified 0.3.0's AtomS3R frame
// (src/utility/IMU_Class.inl: axis_order_yxz | axis_invert_y for the
// accelerometer and the gyroscope).
#include "m5_atoms3/imu.h"

#include "bmi270.h"
#include "esp_check.h"
#include "imu_chips.h"
#include "m5_atoms3/board.h"

static const char *TAG = "m5_atoms3_imu";

// BMI270 at +-4 g and +-2000 dps (src/bmi270.c).
#define BMI270_LSB_PER_G 8192.0f
#define BMI270_LSB_PER_DPS 16.384f

static i2c_master_bus_handle_t s_internal_bus;

esp_err_t m5_atoms3_i2c_internal_init(i2c_master_bus_handle_t *out) {
  ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "out is NULL");
  if (!s_internal_bus) {
    const bool r = m5_atoms3_board() == M5_ATOMS3_BOARD_ATOMS3R;
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num =
            r ? M5_ATOMS3R_I2C_INTERNAL_SDA : M5_ATOMS3_I2C_INTERNAL_SDA,
        .scl_io_num =
            r ? M5_ATOMS3R_I2C_INTERNAL_SCL : M5_ATOMS3_I2C_INTERNAL_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &s_internal_bus), TAG,
                        "internal bus");
  }
  *out = s_internal_bus;
  return ESP_OK;
}

esp_err_t m5_atoms3_imu_init(i2c_master_bus_handle_t bus,
                             m5_atoms3_imu_t *out) {
  ESP_RETURN_ON_FALSE(bus && out, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
  if (m5_atoms3_board() == M5_ATOMS3_BOARD_ATOMS3) {
    out->chip = M5_ATOMS3_IMU_MPU6886;
    return m5_atoms3_mpu6886_init(bus, out);
  }
  out->chip = M5_ATOMS3_IMU_BMI270;
  out->dev = NULL;
  out->who_am_i = 0;
  ESP_RETURN_ON_ERROR(m5_atoms3_bmi270_begin(bus, M5_ATOMS3R_BMI270_ADDRESS),
                      TAG, "BMI270");
  ESP_LOGI(TAG, "BMI270 ready");
  return ESP_OK;
}

esp_err_t m5_atoms3_imu_read(const m5_atoms3_imu_t *imu,
                             m5_atoms3_imu_sample_t *out) {
  ESP_RETURN_ON_FALSE(imu && out, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
  if (imu->chip == M5_ATOMS3_IMU_MPU6886) {
    return m5_atoms3_mpu6886_read(imu, out);
  }
  m5_atoms3_bmi270_raw_t raw;
  ESP_RETURN_ON_ERROR(m5_atoms3_bmi270_read(&raw), TAG, "read");
  out->ax = (float)raw.acc[1] / BMI270_LSB_PER_G;
  out->ay = -(float)raw.acc[0] / BMI270_LSB_PER_G;
  out->az = (float)raw.acc[2] / BMI270_LSB_PER_G;
  out->gx = (float)raw.gyr[1] / BMI270_LSB_PER_DPS;
  out->gy = -(float)raw.gyr[0] / BMI270_LSB_PER_DPS;
  out->gz = (float)raw.gyr[2] / BMI270_LSB_PER_DPS;
  return ESP_OK;
}

void m5_atoms3_imu_deinit(m5_atoms3_imu_t *imu) {
  if (imu && imu->chip == M5_ATOMS3_IMU_MPU6886) {
    m5_atoms3_mpu6886_deinit(imu);
  }
}
