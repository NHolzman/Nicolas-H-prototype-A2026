#ifndef I2C_H_
#define I2C_H_

#include <avr/io.h>

#ifdef __cplusplus
extern "C" {
	#endif

	void initI2C(void);
	void i2cWaitForComplete(void);
	void i2cStart(void);
	void i2cStop(void);
	void i2cSend(uint8_t data);
	uint8_t i2cReadAck(void);
	uint8_t i2cReadNoAck(void);

	#ifdef __cplusplus
}
#endif

#endif /* I2C_H_ */