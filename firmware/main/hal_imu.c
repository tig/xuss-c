/* MPU6886 IMU driver — device headers allowed here (hal_imu).
 * Internal M5GO I2C bus: SDA=21, SCL=22 @ 400 kHz, slave addr 0x68.
 * Ranges: accel +/-8g (4096 LSB/g), gyro +/-2000 dps (16.4 LSB/dps),
 * temp = raw/326.8 + 25 C. */
#include "hal_imu.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>

#define IMU_PORT I2C_NUM_0
#define IMU_SDA 21
#define IMU_SCL 22
#define IMU_ADDR 0x68

#define REG_WHOAMI 0x75
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_CONFIG 0x1C
#define REG_GYRO_CONFIG 0x1B
#define REG_ACCEL_XOUT_H 0x3B

static const char *TAG = "hal_imu";
static int s_ok = 0;

static esp_err_t wr(uint8_t reg, uint8_t val) {
  uint8_t buf[2] = {reg, val};
  return i2c_master_write_to_device(IMU_PORT, IMU_ADDR, buf, sizeof buf,
                                    pdMS_TO_TICKS(50));
}

static esp_err_t rd(uint8_t reg, uint8_t *out, size_t n) {
  return i2c_master_write_read_device(IMU_PORT, IMU_ADDR, &reg, 1, out, n,
                                      pdMS_TO_TICKS(50));
}

void gcu_imu_init(void) {
  i2c_config_t cfg = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = IMU_SDA,
      .scl_io_num = IMU_SCL,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 400000,
  };
  if (i2c_param_config(IMU_PORT, &cfg) != ESP_OK ||
      i2c_driver_install(IMU_PORT, cfg.mode, 0, 0, 0) != ESP_OK) {
    ESP_LOGW(TAG, "i2c init failed");
    return;
  }

  uint8_t who = 0;
  if (rd(REG_WHOAMI, &who, 1) != ESP_OK) {
    ESP_LOGW(TAG, "WHO_AM_I read failed");
    return;
  }
  ESP_LOGI(TAG, "WHO_AM_I=0x%02x", who); /* MPU6886 == 0x19 */

  wr(REG_PWR_MGMT_1, 0x80); /* device reset */
  vTaskDelay(pdMS_TO_TICKS(20));
  wr(REG_PWR_MGMT_1, 0x01); /* wake, PLL clock */
  vTaskDelay(pdMS_TO_TICKS(10));
  wr(REG_ACCEL_CONFIG, 0x10); /* +/-8g */
  wr(REG_GYRO_CONFIG, 0x18);  /* +/-2000 dps */
  vTaskDelay(pdMS_TO_TICKS(10));
  s_ok = 1;
}

int gcu_imu_read(gcu_hal_t *self, long accel_mg[3], long gyro_mdps[3],
                 long *temp_mc) {
  (void)self;
  if (!s_ok) {
    return 0;
  }
  uint8_t b[14];
  if (rd(REG_ACCEL_XOUT_H, b, sizeof b) != ESP_OK) {
    return 0;
  }
  int16_t ax = (int16_t)((b[0] << 8) | b[1]);
  int16_t ay = (int16_t)((b[2] << 8) | b[3]);
  int16_t az = (int16_t)((b[4] << 8) | b[5]);
  int16_t traw = (int16_t)((b[6] << 8) | b[7]);
  int16_t gx = (int16_t)((b[8] << 8) | b[9]);
  int16_t gy = (int16_t)((b[10] << 8) | b[11]);
  int16_t gz = (int16_t)((b[12] << 8) | b[13]);

  /* accel: 4096 LSB/g -> milli-g = raw*1000/4096 */
  accel_mg[0] = (long)ax * 1000 / 4096;
  accel_mg[1] = (long)ay * 1000 / 4096;
  accel_mg[2] = (long)az * 1000 / 4096;
  /* gyro: 16.4 LSB/dps -> milli-dps = raw*10000/164 */
  gyro_mdps[0] = (long)gx * 10000 / 164;
  gyro_mdps[1] = (long)gy * 10000 / 164;
  gyro_mdps[2] = (long)gz * 10000 / 164;
  /* temp C = raw/326.8 + 25 -> milli-C = raw*10000/3268 + 25000 */
  *temp_mc = (long)traw * 10000 / 3268 + 25000;
  return 1;
}
