// SPDX-FileCopyrightText: 2026 m5stack-firmware contributors
// SPDX-License-Identifier: MIT
//
// Board identification as M5GFX 0.2.19 src/M5GFX.cpp autodetects an
// ESP32-S3: package version 0 (QFN56) holds the AtomS3 candidates, 1
// (ESP32-S3-PICO, LGA56) the AtomS3R.
#include "m5_atoms3/board.h"

#include "esp_efuse.h"
#include "soc/efuse_defs.h"

m5_atoms3_board_t m5_atoms3_board(void) {
  return esp_efuse_get_pkg_ver() == EFUSE_PKG_VERSION_ESP32S3PICO
             ? M5_ATOMS3_BOARD_ATOMS3R
             : M5_ATOMS3_BOARD_ATOMS3;
}

const char *m5_atoms3_board_name(void) {
  return m5_atoms3_board() == M5_ATOMS3_BOARD_ATOMS3R ? "AtomS3R" : "AtomS3";
}
