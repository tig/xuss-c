/* Device sensor backend — allowlisted for device headers.
 * MPU6886 6-axis IMU on the M5 core internal I2C (addr 0x68).
 * Configured to +/-8 g and +/-2000 dps; scaling below matches. */
#include "sensor_board.h"

#include "gcu/defaults.h"

#include "driver/i2c_master.h"

#include <stdio.h>

#define MPU_ADDR 0x68
#define REG_WHO_AM_I 0x75
#define WHO_AM_I_MPU6886 0x19
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_CONFIG 0x1C
#define REG_GYRO_CONFIG 0x1B
#define REG_ACCEL_XOUT_H 0x3B

static i2c_master_dev_handle_t dev;

static int wr(unsigned char reg, unsigned char val) {
  unsigned char b[2] = {reg, val};
  return i2c_master_transmit(dev, b, 2, 100) == ESP_OK;
}

static int rd(unsigned char reg, unsigned char *dst, int n) {
  return i2c_master_transmit_receive(dev, &reg, 1, dst, (size_t)n, 100) ==
         ESP_OK;
}

int gcu_board_imu_init(void) {
  i2c_master_bus_config_t bus_cfg = {
      .i2c_port = 0,
      .sda_io_num = GCU_DEFAULTS.i2c_sda_pin,
      .scl_io_num = GCU_DEFAULTS.i2c_scl_pin,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  i2c_master_bus_handle_t bus = NULL;
  if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
    printf("imu=missing\n");
    return 0;
  }
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = MPU_ADDR,
      .scl_speed_hz = 400000,
  };
  if (i2c_master_bus_add_device(bus, &dev_cfg, &dev) != ESP_OK) {
    printf("imu=missing\n");
    return 0;
  }
  unsigned char who = 0;
  if (!rd(REG_WHO_AM_I, &who, 1) || who != WHO_AM_I_MPU6886) {
    printf("imu=missing who=0x%02x\n", who);
    dev = NULL;
    return 0;
  }
  if (!wr(REG_PWR_MGMT_1, 0x01) ||   /* wake, PLL clock */
      !wr(REG_ACCEL_CONFIG, 0x10) || /* +/-8 g */
      !wr(REG_GYRO_CONFIG, 0x18)) {  /* +/-2000 dps */
    printf("imu=missing\n");
    dev = NULL;
    return 0;
  }
  printf("imu=ok\n");
  return 1;
}

int gcu_board_imu_read(int mg[3], int dps[3], int *temp_dc) {
  unsigned char b[14];
  if (!dev || !rd(REG_ACCEL_XOUT_H, b, 14)) {
    return 0;
  }
  for (int i = 0; i < 3; i++) {
    int a = (short)((b[2 * i] << 8) | b[2 * i + 1]);
    int g = (short)((b[8 + 2 * i] << 8) | b[9 + 2 * i]);
    mg[i] = a * 8000 / 32768;
    dps[i] = g * 2000 / 32768;
  }
  int traw = (short)((b[6] << 8) | b[7]);
  /* MPU6886: T(degC) = raw/326.8 + 25 -> deci-degC */
  *temp_dc = traw * 100 / 3268 + 250;
  return 1;
}
