/* MPU6886 on M5GO internal I2C (knowledge/m5-core.md). */
#include "imu.h"

#include "gcu/defaults.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <math.h>

static const char *TAG = "imu";
#define MPU_ADDR 0x68
#define REG_WHOAMI 0x75
#define REG_PWR 0x6B
#define REG_ACCEL 0x3B
#define REG_GYRO 0x43
#define REG_TEMP 0x41

static int s_ready;

static esp_err_t wr(uint8_t reg, uint8_t val) {
  uint8_t buf[2] = {reg, val};
  return i2c_master_write_to_device(I2C_NUM_0, MPU_ADDR, buf, 2,
                                    pdMS_TO_TICKS(50));
}

static esp_err_t rd(uint8_t reg, uint8_t *data, size_t n) {
  return i2c_master_write_read_device(I2C_NUM_0, MPU_ADDR, &reg, 1, data, n,
                                      pdMS_TO_TICKS(50));
}

int imu_init(void) {
  if (s_ready) {
    return 0;
  }
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = GCU_PIN_I2C_SDA,
      .scl_io_num = GCU_PIN_I2C_SCL,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 400000,
  };
  ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));

  uint8_t who = 0;
  if (rd(REG_WHOAMI, &who, 1) != ESP_OK || who != 0x19) {
    ESP_LOGW(TAG, "MPU6886 not found (who=0x%02x)", who);
    return -1;
  }
  /* Wake */
  if (wr(REG_PWR, 0x00) != ESP_OK) {
    return -1;
  }
  s_ready = 1;
  ESP_LOGI(TAG, "MPU6886 ready who=0x%02x", who);
  return 0;
}

static int16_t be16(const uint8_t *p) {
  return (int16_t)((p[0] << 8) | p[1]);
}

int imu_read(imu_sample_t *out) {
  if (!out) {
    return -1;
  }
  out->present = 0;
  if (!s_ready) {
    return -1;
  }
  uint8_t raw[14];
  if (rd(REG_ACCEL, raw, 14) != ESP_OK) {
    return -1;
  }
  /* accel ±2g default ≈ 16384 LSB/g */
  out->ax = be16(&raw[0]) / 16384.0f;
  out->ay = be16(&raw[2]) / 16384.0f;
  out->az = be16(&raw[4]) / 16384.0f;
  int16_t t = be16(&raw[6]);
  /* MPU6886 temp: raw/326.8 + 25 (not 6050 formula). */
  out->temp_c = (t / 326.8f) + 25.0f;
  /* gyro ±250 dps default ≈ 131 LSB/(°/s) */
  out->gx = be16(&raw[8]) / 131.0f;
  out->gy = be16(&raw[10]) / 131.0f;
  out->gz = be16(&raw[12]) / 131.0f;
  out->present = 1;
  return 0;
}
