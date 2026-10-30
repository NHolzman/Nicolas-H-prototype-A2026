#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include "i2c.h"

void initI2C(void) {
	// Activer les resistances de rappel internes sur PC4 (SDA) et PC5 (SCL)
	PORTC |= (1 << PORTC4) | (1 << PORTC5);

	// Prescaler = 1
	TWSR &= ~((1 << TWPS1) | (1 << TWPS0));

	// 8MHz / (16 + 2 * TWBR * 1) ~= 100kHz
	TWBR = 32;

	// Activer le module TWI
	TWCR = (1 << TWEN);
}

void i2cWaitForComplete(void) {
	uint16_t timeout = 25000;
	while (!(TWCR & (1 << TWINT)) && --timeout);
}

void i2cStart(void) {
	TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWSTA));
	i2cWaitForComplete();
}

void i2cStop(void) {
	TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWSTO));
	uint16_t timeout = 5000;
	while ((TWCR & _BV(TWSTO)) && --timeout);
}

uint8_t i2cReadAck(void) {
	TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWEA));
	i2cWaitForComplete();
	return TWDR;
}

uint8_t i2cReadNoAck(void) {
	TWCR = (_BV(TWINT) | _BV(TWEN));
	i2cWaitForComplete();
	return TWDR;
}

void i2cSend(uint8_t data) {
	TWDR = data;
	TWCR = (_BV(TWINT) | _BV(TWEN));
	i2cWaitForComplete();
}