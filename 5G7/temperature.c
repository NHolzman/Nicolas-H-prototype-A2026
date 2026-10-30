#include <stdio.h>
#include <stdlib.h>
#include "bme280.h"
#include "common.h"

int8_t get_temperature(uint32_t period, struct bme280_dev *dev)
{
    (void)period;
    int32_t t_int = 0, t_dec = 0;
    uint32_t h_int = 0, h_dec = 0;
    uint32_t p_int = 0, p_dec = 0;
    return bme280_read_data(dev, &t_int, &t_dec, &h_int, &h_dec, &p_int, &p_dec);
}