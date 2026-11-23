// SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
// SPDX-License-Identifier: MIT
//
// The IMU drivers behind m5_atoms3/imu.h.
#pragma once

#include "m5_atoms3/imu.h"

esp_err_t m5_atoms3_mpu6886_init(i2c_master_bus_handle_t bus,
                                 m5_atoms3_imu_t *out);
esp_err_t m5_atoms3_mpu6886_read(const m5_atoms3_imu_t *imu,
                                 m5_atoms3_imu_sample_t *out);
void m5_atoms3_mpu6886_deinit(m5_atoms3_imu_t *imu);
