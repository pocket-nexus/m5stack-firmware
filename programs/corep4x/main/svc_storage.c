/* SD CARD window: the FAT card on SPI3 at /sdcard (the factory's SPI mode,
 * which leaves the SDMMC host to the Wi-Fi module), card detect on IOE1
 * IO13, the directory listing and the 512 KiB speed test of
 * ui_function_sdcard_window.cpp. */
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "m5_corep4x/bsp.h"
#include "sdmmc_cmd.h"
#include "svc.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "svc_storage";

#define MOUNT_POINT "/sdcard"
#define SD_HOST SPI3_HOST
#define SD_KHZ 20000
#define MAX_ENTRIES 48
#define NAME_MAX_CHARS 71
#define POLL_MS 1000
#define DEBOUNCE_MS 300
#define TEST_FILE MOUNT_POINT "/.ui_test_block"
#define TEST_BYTES 524288U
#define TEST_CHUNK 4096U
#define LISTING_BYTES 4096

typedef enum { CMD_LIST, CMD_TEST } cmd_kind_t;

typedef struct {
  cmd_kind_t kind;
  char path[128];
} cmd_t;

typedef struct {
  bool dir;
  uint64_t size;
  char name[NAME_MAX_CHARS + 1];
} entry_t;

static QueueHandle_t s_queue;
/* Held around every mount, unmount and card access: the sleep sequence
 * unmounts from another task. */
static SemaphoreHandle_t s_card_lock;
static sdmmc_card_t *s_card;
static bool s_bus;
static bool s_inserted;
static volatile bool s_sleeping;

static void format_bytes(uint64_t bytes, char *out, size_t size) {
  const double v = (double)bytes;
  const double k = 1024.0;
  if (v >= k * k * k * k) {
    snprintf(out, size, "%.2f TB", v / (k * k * k * k));
  } else if (v >= k * k * k) {
    snprintf(out, size, "%.2f GB", v / (k * k * k));
  } else if (v >= k * k) {
    snprintf(out, size, "%.2f MB", v / (k * k));
  } else if (v >= k) {
    snprintf(out, size, "%.2f KB", v / k);
  } else {
    snprintf(out, size, "%llu B", (unsigned long long)bytes);
  }
}

static bool rail_l3b(void) {
  bool on = false;
  return m5_corep4x_get_switch(M5_COREP4X_SWITCH_L3B, &on) == ESP_OK && on;
}

static bool ready(void) { return s_card && s_inserted && rail_l3b(); }

/* refresh_sd_status: free / total GiB. */
static void publish_status(void) {
  const bool ok = ready();
  m5_pocket_bridge_set_bool("sdReady", ok);
  uint64_t total = 0;
  uint64_t free_bytes = 0;
  if (ok && esp_vfs_fat_info(MOUNT_POINT, &total, &free_bytes) == ESP_OK) {
    char text[48];
    const double gib = 1024.0 * 1024.0 * 1024.0;
    snprintf(text, sizeof(text), "%.2f / %.2f GB", (double)free_bytes / gib,
             (double)total / gib);
    m5_pocket_bridge_set_text("sdStatus", text);
  } else {
    m5_pocket_bridge_set_text("sdStatus", "SD: Not Ready");
  }
}

static void unmount(void) {
  if (s_card) {
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    s_card = NULL;
  }
  if (s_bus) {
    spi_bus_free(SD_HOST);
    s_bus = false;
  }
}

/* esp_vfs_fat_sdspi_mount with max_files 5, 64 KiB allocation unit, no
 * format; the bus is freed when the mount fails. */
static void mount(void) {
  if (s_card) {
    return;
  }
  const spi_bus_config_t bus = {
      .mosi_io_num = M5_COREP4X_SD_MOSI,
      .miso_io_num = M5_COREP4X_SD_MISO,
      .sclk_io_num = M5_COREP4X_SD_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 2048,
  };
  esp_err_t err = spi_bus_initialize(SD_HOST, &bus, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "SPI bus: %s", esp_err_to_name(err));
    return;
  }
  s_bus = true;
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SD_HOST;
  host.max_freq_khz = SD_KHZ;
  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = M5_COREP4X_SD_CS;
  slot.host_id = SD_HOST;
  const esp_vfs_fat_mount_config_t mount = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 64 * 1024,
  };
  err = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot, &mount, &s_card);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mount: %s", esp_err_to_name(err));
    s_card = NULL;
    unmount();
    return;
  }
  ESP_LOGI(TAG, "card mounted: %s", s_card->cid.name);
}

static int compare_entries(const void *a, const void *b) {
  const entry_t *x = a;
  const entry_t *y = b;
  if (x->dir != y->dir) {
    return x->dir ? -1 : 1;
  }
  return strcmp(x->name, y->name);
}

/* The first 48 entries in readdir order, stat'ed, directories first, then
 * by name. */
static void list(const char *path) {
  if (!ready()) {
    m5_pocket_bridge_set_text("sdListing", "");
    m5_pocket_bridge_set_text("sdPathError", "");
    m5_pocket_bridge_set_text("sdFolderSize", "0 B");
    publish_status();
    m5_pocket_bridge_post_event("sdListed", 0);
    return;
  }
  entry_t *entries = calloc(MAX_ENTRIES, sizeof(entry_t));
  char *text = malloc(LISTING_BYTES);
  DIR *dir = entries && text ? opendir(path) : NULL;
  size_t count = 0;
  uint64_t folder = 0;
  if (!dir) {
    m5_pocket_bridge_set_text("sdPathError", "open dir failed");
  } else {
    m5_pocket_bridge_set_text("sdPathError", "");
    struct dirent *e;
    while (count < MAX_ENTRIES && (e = readdir(dir)) != NULL) {
      if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
        continue;
      }
      entry_t *entry = &entries[count++];
      const size_t length = strnlen(e->d_name, NAME_MAX_CHARS);
      memcpy(entry->name, e->d_name, length);
      entry->name[length] = '\0';
      char full[sizeof(((cmd_t *)0)->path) + sizeof(e->d_name) + 1];
      snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
      struct stat st;
      if (stat(full, &st) == 0) {
        entry->dir = S_ISDIR(st.st_mode);
        entry->size = entry->dir ? 0 : (uint64_t)st.st_size;
      }
      if (!entry->dir) {
        folder += entry->size;
      }
    }
    closedir(dir);
    qsort(entries, count, sizeof(entry_t), compare_entries);
  }
  size_t used = 0;
  if (text) {
    text[0] = '\0';
    for (size_t i = 0; i < count && used < LISTING_BYTES; i++) {
      char size[24];
      format_bytes(entries[i].size, size, sizeof(size));
      const int n = snprintf(text + used, LISTING_BYTES - used, "%c%s\t%.48s\n",
                             entries[i].dir ? 'D' : 'F', size, entries[i].name);
      if (n < 0 || (size_t)n >= LISTING_BYTES - used) {
        break;
      }
      used += (size_t)n;
    }
    text[used] = '\0';
    m5_pocket_bridge_set_text("sdListing", text);
  }
  char size[24];
  format_bytes(folder, size, sizeof(size));
  m5_pocket_bridge_set_text("sdFolderSize", size);
  publish_status();
  m5_pocket_bridge_post_event("sdListed", (double)count);
  free(entries);
  free(text);
}

/* ---- speed test ------------------------------------------------------------
 */

static uint32_t checksum_step(uint32_t c, uint8_t byte) {
  return (c * 33U) ^ byte;
}

static void test_write(char *out, size_t size, uint32_t *checksum,
                       bool *written) {
  *written = false;
  uint8_t *buf = malloc(TEST_CHUNK);
  FILE *f = buf ? fopen(TEST_FILE, "wb") : NULL;
  if (!buf) {
    snprintf(out, size, "Write failed: no memory");
    return;
  }
  if (!f) {
    free(buf);
    snprintf(out, size, "Write failed: cannot open %s", TEST_FILE);
    return;
  }
  const int64_t start = esp_timer_get_time();
  uint32_t c = 0;
  uint32_t done = 0;
  while (done < TEST_BYTES) {
    for (uint32_t i = 0; i < TEST_CHUNK; i++) {
      buf[i] = (uint8_t)((done + i) & 0xFF);
      c = checksum_step(c, buf[i]);
    }
    if (fwrite(buf, 1, TEST_CHUNK, f) != TEST_CHUNK) {
      break;
    }
    done += TEST_CHUNK;
  }
  fclose(f);
  const double seconds = (double)(esp_timer_get_time() - start) / 1e6;
  free(buf);
  if (done < TEST_BYTES) {
    snprintf(out, size, "Write failed at %u/%u bytes", (unsigned)done,
             (unsigned)TEST_BYTES);
    return;
  }
  *checksum = c;
  *written = true;
  snprintf(out, size,
           "Write OK: %u bytes to %s\nSpeed: %.1f kB/s\nChecksum: 0x%08lX",
           (unsigned)TEST_BYTES, TEST_FILE,
           seconds > 0 ? (TEST_BYTES / 1024.0) / seconds : 0.0,
           (unsigned long)c);
}

static void test_read(char *out, size_t size, bool written, uint32_t expected) {
  FILE *f = fopen(TEST_FILE, "rb");
  if (!f) {
    snprintf(out, size,
             "Read failed: test file not found. Run Write Test "
             "first.");
    return;
  }
  uint8_t *buf = malloc(TEST_CHUNK);
  if (!buf) {
    fclose(f);
    snprintf(out, size, "Read failed: no memory");
    return;
  }
  const int64_t start = esp_timer_get_time();
  uint32_t c = 0;
  uint32_t total = 0;
  size_t n;
  while ((n = fread(buf, 1, TEST_CHUNK, f)) > 0) {
    for (size_t i = 0; i < n; i++) {
      c = checksum_step(c, buf[i]);
    }
    total += (uint32_t)n;
  }
  fclose(f);
  const double seconds = (double)(esp_timer_get_time() - start) / 1e6;
  free(buf);
  snprintf(out, size,
           "Read OK: %u bytes from %s\nSpeed: %.1f kB/s\nChecksum: 0x%08lX "
           "(%s)",
           (unsigned)total, TEST_FILE,
           seconds > 0 ? (total / 1024.0) / seconds : 0.0, (unsigned long)c,
           !written || c == expected ? "OK" : "MISMATCH");
}

static void speed_test(void) {
  char *text = malloc(1024);
  char *w = malloc(256);
  char *r = malloc(256);
  if (!text || !w || !r) {
    free(text);
    free(w);
    free(r);
    m5_pocket_bridge_set_text("sdTestText",
                              "SD test failed: cannot create worker task.");
    m5_pocket_bridge_post_event("sdTest", 0);
    return;
  }
  if (!ready()) {
    m5_pocket_bridge_set_text(
        "sdTestText",
        "SD is not ready. Check L3B, card detect and mount state.");
  } else {
    m5_pocket_bridge_set_text("sdTestText",
                              "SD speed test\n1/3 Write 512 KB...");
    uint32_t checksum = 0;
    bool written = false;
    test_write(w, 256, &checksum, &written);
    publish_status();
    snprintf(text, 1024, "SD speed test\n[Write]\n%s\n\n2/3 Read...", w);
    m5_pocket_bridge_set_text("sdTestText", text);
    test_read(r, 256, written, checksum);
    publish_status();
    snprintf(text, 1024,
             "SD speed test\n[Write]\n%s\n\n[Read]\n%s\n\n3/3 Cleanup...", w,
             r);
    m5_pocket_bridge_set_text("sdTestText", text);
    const bool removed = remove(TEST_FILE) == 0;
    publish_status();
    snprintf(text, 1024,
             "SD speed test done\n[Write]\n%s\n\n[Read]\n%s\n\n[Cleanup]\n%s",
             w, r,
             removed ? "Deleted " TEST_FILE
                     : "Delete skipped/failed: " TEST_FILE " not found");
    m5_pocket_bridge_set_text("sdTestText", text);
  }
  m5_pocket_bridge_post_event("sdTest", 1);
  free(text);
  free(w);
  free(r);
}

/* ---- task -------------------------------------------------------------------
 */

/* The factory's hot-plug task: 300 ms debounce, then unmount on removal or
 * mount on insertion. Losing L3B unmounts as well. */
static void poll_card(void) {
  bool inserted = false;
  if (m5_corep4x_sd_detected(&inserted) != ESP_OK) {
    return;
  }
  const bool powered = rail_l3b();
  const bool want = inserted && powered;
  if (want == (s_card != NULL) && inserted == s_inserted) {
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
  if (m5_corep4x_sd_detected(&inserted) != ESP_OK) {
    return;
  }
  xSemaphoreTake(s_card_lock, portMAX_DELAY);
  if (!s_sleeping) {
    s_inserted = inserted;
    if (inserted && powered) {
      mount();
    } else {
      unmount();
    }
    publish_status();
  }
  xSemaphoreGive(s_card_lock);
}

static void storage_task(void *arg) {
  (void)arg;
  poll_card();
  publish_status();
  for (;;) {
    cmd_t cmd;
    if (xQueueReceive(s_queue, &cmd, pdMS_TO_TICKS(POLL_MS)) == pdTRUE) {
      xSemaphoreTake(s_card_lock, portMAX_DELAY);
      if (!s_sleeping) {
        if (cmd.kind == CMD_LIST) {
          list(cmd.path);
        } else {
          speed_test();
        }
      }
      xSemaphoreGive(s_card_lock);
    } else if (!s_sleeping) {
      poll_card();
    }
  }
}

bool storage_svc_request(const m5_pocket_request *request) {
  cmd_t cmd = {0};
  if (svc_is(request, "sdList")) {
    cmd.kind = CMD_LIST;
    snprintf(cmd.path, sizeof(cmd.path), "%s", svc_rest(request));
  } else if (svc_is(request, "sdTest")) {
    cmd.kind = CMD_TEST;
  } else {
    return false;
  }
  if (!s_queue || xQueueSend(s_queue, &cmd, 0) != pdTRUE) {
    if (cmd.kind == CMD_TEST) {
      m5_pocket_bridge_set_text("sdTestText",
                                "SD test failed: cannot create worker task.");
      m5_pocket_bridge_post_event("sdTest", 0);
    }
  }
  return true;
}

void storage_svc_frame(void) {}

void storage_svc_sleep(void) {
  /* Waits for a listing or speed test in progress. */
  if (s_card_lock) {
    xSemaphoreTake(s_card_lock, portMAX_DELAY);
  }
  s_sleeping = true;
  unmount();
  static const gpio_num_t pins[] = {M5_COREP4X_SD_CS, M5_COREP4X_SD_MOSI,
                                    M5_COREP4X_SD_MISO, M5_COREP4X_SD_CLK};
  for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    m5_corep4x_gpio_high_z(pins[i]);
  }
  m5_corep4x_sd_detect_enable(false);
  if (s_card_lock) {
    xSemaphoreGive(s_card_lock);
  }
}

void storage_svc_start(void) {
  m5_pocket_bridge_set_bool("sdReady", false);
  m5_pocket_bridge_set_text("sdStatus", "SD: Not Ready");
  s_queue = xQueueCreate(4, sizeof(cmd_t));
  s_card_lock = xSemaphoreCreateMutex();
  if (!s_queue || !s_card_lock ||
      xTaskCreate(storage_task, "sd_hotplug", 6144, NULL, 5, NULL) != pdPASS) {
    ESP_LOGE(TAG, "no storage task");
  }
}
