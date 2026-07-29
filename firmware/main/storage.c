#include "storage.h"

#include "esp_log.h"
#include "esp_spiffs.h"

#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "storage";
static int s_ready;

int storage_init(void) {
  if (s_ready) {
    return 0;
  }
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 4,
      .format_if_mount_failed = false,
  };
  esp_err_t err = esp_vfs_spiffs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
    return -1;
  }
  size_t total = 0, used = 0;
  if (esp_spiffs_info("storage", &total, &used) == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS total=%u used=%u", (unsigned)total, (unsigned)used);
  }
  s_ready = 1;
  return 0;
}

const char *storage_first_pcm_path(void) { return "/spiffs/first.pcm"; }

size_t storage_first_pcm_size(void) {
  if (!s_ready) {
    return 0;
  }
  struct stat st;
  if (stat(storage_first_pcm_path(), &st) != 0 || st.st_size <= 0) {
    return 0;
  }
  return (size_t)st.st_size;
}
