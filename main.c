#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "bmi270.h"

int fd;

BMI2_INTF_RETURN_TYPE bmi2_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data msgset[1];

    msgs[0].addr = 0x68;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &reg_addr;

    msgs[1].addr = 0x68;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    msgs[1].buf = reg_data;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 2;

    if (ioctl(fd, I2C_RDWR, &msgset) < 0) return BMI2_E_COM_FAIL;
    return BMI2_OK;
}

BMI2_INTF_RETURN_TYPE bmi2_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    uint8_t *buf = malloc(len + 1);
    buf[0] = reg_addr;
    for (uint32_t i = 0; i < len; i++) buf[i + 1] = reg_data[i];

    struct i2c_msg msgs[1];
    struct i2c_rdwr_ioctl_data msgset[1];

    msgs[0].addr = 0x68;
    msgs[0].flags = 0;
    msgs[0].len = len + 1;
    msgs[0].buf = buf;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 1;

    if (ioctl(fd, I2C_RDWR, &msgset) < 0) {
        free(buf);
        return BMI2_E_COM_FAIL;
    }
    free(buf);
    return BMI2_OK;
}

void bmi2_delay_us(uint32_t period, void *intf_ptr) {
    usleep(period);
}

int main() {
    struct bmi2_dev bmi;
    struct bmi2_sens_data sensor_data;
    int8_t rslt;

    fd = open("/dev/i2c-3", O_RDWR);
    if (fd < 0) {
        printf("Failed to open /dev/i2c-3.\n");
        return -1;
    }

    bmi.intf = BMI2_I2C_INTF;
    bmi.read = bmi2_i2c_read;
    bmi.write = bmi2_i2c_write;
    bmi.delay_us = bmi2_delay_us;
    bmi.read_write_len = 32;
    bmi.intf_ptr = NULL;
    bmi.config_file_ptr = NULL; 

    printf("Initializing BMI270...\n");
    rslt = bmi270_init(&bmi);
    if (rslt != BMI2_OK) {
        printf("Init failed with error code: %d\n", rslt);
        return -1;
    }

    uint8_t sens_list[2] = {BMI2_ACCEL, BMI2_GYRO};
    bmi2_sensor_enable(sens_list, 2, &bmi);
    printf("Initialization successful!\n");

    FILE *f = fopen("/mnt/mmcblk0p1/imu_log.csv", "w");
    fprintf(f, "Accel_X,Accel_Y,Accel_Z,Gyro_X,Gyro_Y,Gyro_Z\n");

    while (1) {
        bmi2_get_sensor_data(&sensor_data, &bmi);
        fprintf(f, "%d,%d,%d,%d,%d,%d\n",
            sensor_data.acc.x, sensor_data.acc.y, sensor_data.acc.z,
            sensor_data.gyr.x, sensor_data.gyr.y, sensor_data.gyr.z);
        fflush(f);
        usleep(10000);
    }

    close(fd);
    return 0;
}
