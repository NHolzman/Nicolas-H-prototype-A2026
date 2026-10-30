#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "i2c.h"
#include "bme280.h"
#include "common.h"

static uint8_t dev_addr = BME280_I2C_ADDR_PRIM;

BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    uint8_t addr = *(uint8_t *)intf_ptr;

    i2cStart();
    i2cSend((addr << 1) | 0x00);
    if ((TWSR & 0xF8) != 0x18) {
        i2cStop();
        return BME280_E_COMM_FAIL;
    }

    i2cSend(reg_addr);
    if ((TWSR & 0xF8) != 0x28) {
        i2cStop();
        return BME280_E_COMM_FAIL;
    }

    i2cStart();
    i2cSend((addr << 1) | 0x01);
    if ((TWSR & 0xF8) != 0x40) {
        i2cStop();
        return BME280_E_COMM_FAIL;
    }

    for (uint32_t i = 0; i < length; i++) {
        if (i == (length - 1)) {
            reg_data[i] = i2cReadNoAck();
        } else {
            reg_data[i] = i2cReadAck();
        }
    }

    i2cStop();
    return BME280_OK;
}

BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    uint8_t addr = *(uint8_t *)intf_ptr;

    i2cStart();
    i2cSend((addr << 1) | 0x00);
    if ((TWSR & 0xF8) != 0x18) {
        i2cStop();
        return BME280_E_COMM_FAIL;
    }

    i2cSend(reg_addr);
    if ((TWSR & 0xF8) != 0x28) {
        i2cStop();
        return BME280_E_COMM_FAIL;
    }

    for (uint32_t i = 0; i < length; i++) {
        i2cSend(reg_data[i]);
        if ((TWSR & 0xF8) != 0x28) {
            i2cStop();
            return BME280_E_COMM_FAIL;
        }
    }

    i2cStop();
    return BME280_OK;
}

BME280_INTF_RET_TYPE bme280_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    (void)reg_addr;
    (void)reg_data;
    (void)length;
    (void)intf_ptr;
    return BME280_E_COMM_FAIL;
}

BME280_INTF_RET_TYPE bme280_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    (void)reg_addr;
    (void)reg_data;
    (void)length;
    (void)intf_ptr;
    return BME280_E_COMM_FAIL;
}

void bme280_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    while (period >= 1000) {
        _delay_ms(1);
        period -= 1000;
    }
    while (period > 0) {
        _delay_us(1);
        period--;
    }
}

int8_t bme280_interface_selection(struct bme280_dev *dev, uint8_t intf)
{
    int8_t rslt = BME280_OK;

    if (dev != NULL)
    {
        initI2C();

        dev->read = bme280_i2c_read;
        dev->write = bme280_i2c_write;
        dev->delay_us = bme280_delay_us;
        dev->intf = BME280_I2C_INTF;

        // Tester d'abord l'adresse I2C primaire (0x76)
        dev_addr = BME280_I2C_ADDR_PRIM;
        dev->intf_ptr = &dev_addr;
        rslt = bme280_init(dev);

        // Si l'adresse primaire echoue, tester l'adresse secondaire (0x77)
        if (rslt != BME280_OK)
        {
            dev_addr = BME280_I2C_ADDR_SEC;
            dev->intf_ptr = &dev_addr;
            rslt = bme280_init(dev);
        }

        // Si le capteur a ete detecte et initialise avec succes
        if (rslt == BME280_OK)
        {
            struct bme280_settings settings;
            settings.osr_h = BME280_OVERSAMPLING_1X;
            settings.osr_p = BME280_OVERSAMPLING_1X;
            settings.osr_t = BME280_OVERSAMPLING_1X;
            settings.filter = BME280_FILTER_COEFF_OFF;
            settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

            uint8_t settings_sel = BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP | BME280_SEL_OSR_HUM | BME280_SEL_FILTER | BME280_SEL_STANDBY;

            rslt = bme280_set_sensor_settings(settings_sel, &settings, dev);
            if (rslt == BME280_OK)
            {
                rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, dev);
            }
        }
    }
    else
    {
        rslt = BME280_E_NULL_PTR;
    }

    return rslt;
}

int8_t bme280_read_data(struct bme280_dev *dev, 
                        int32_t *temp_int, int32_t *temp_dec, 
                        uint32_t *hum_int, uint32_t *hum_dec,
                        uint32_t *press_int, uint32_t *press_dec)
{
    struct bme280_data comp_data;
    int8_t rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev);

    if (rslt == BME280_OK)
    {
        // 1. Temperature (en degres Celsius)
        double temp = comp_data.temperature;
        *temp_int = (int32_t)temp;
        double frac_t = temp - (double)(*temp_int);
        if (frac_t < 0.0) {
            frac_t = -frac_t;
        }
        *temp_dec = (int32_t)(frac_t * 100.0 + 0.5);
        if (*temp_dec >= 100) {
            *temp_dec = 0;
            if (*temp_int >= 0) {
                (*temp_int)++;
            } else {
                (*temp_int)--;
            }
        }

        // 2. Humidite (en pourcentage %RH)
        double hum = comp_data.humidity;
        if (hum < 0.0) {
            hum = 0.0;
        }
        *hum_int = (uint32_t)hum;
        double frac_h = hum - (double)(*hum_int);
        if (frac_h < 0.0) {
            frac_h = 0.0;
        }
        *hum_dec = (uint32_t)(frac_h * 100.0 + 0.5);
        if (*hum_dec >= 100) {
            *hum_dec = 0;
            (*hum_int)++;
        }

        // 3. Pression (convertie de Pa en hPa)
        double press_hpa = comp_data.pressure / 100.0;
        if (press_hpa < 0.0) {
            press_hpa = 0.0;
        }
        *press_int = (uint32_t)press_hpa;
        double frac_p = press_hpa - (double)(*press_int);
        if (frac_p < 0.0) {
            frac_p = 0.0;
        }
        *press_dec = (uint32_t)(frac_p * 100.0 + 0.5);
        if (*press_dec >= 100) {
            *press_dec = 0;
            (*press_int)++;
        }
    }

    return rslt;
}

void bme280_error_codes_print_result(const char api_name[], int8_t rslt)
{
    (void)api_name;
    (void)rslt;
}

void bme280_coines_deinit(void)
{
}