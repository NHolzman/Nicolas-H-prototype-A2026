#ifndef _COMMON_H
#define _COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "bme280.h"

BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
BME280_INTF_RET_TYPE bme280_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
BME280_INTF_RET_TYPE bme280_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);

void bme280_delay_us(uint32_t period_us, void *intf_ptr);
int8_t bme280_interface_selection(struct bme280_dev *dev, uint8_t intf);
int8_t bme280_read_data(struct bme280_dev *dev, 
                        int32_t *temp_int, int32_t *temp_dec, 
                        uint32_t *hum_int, uint32_t *hum_dec,
                        uint32_t *press_int, uint32_t *press_dec);
void bme280_error_codes_print_result(const char api_name[], int8_t rslt);
void bme280_coines_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_H */