/*
- main.cpp
*
- Created: 2024-08-23
- Author : I.Chahed
- Updated: Intégration GP2Y0A02YK sur PC2 (ADC2)
*/

#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <avr/sfr_defs.h>
#include <avr/power.h>
#include "i2c.h"
#include "bme280.h"
#include "common.h"

extern "C" {
	#include "ili9341.h"
	#include "ili9341gfx.h"
};

#define POINTCOLOUR PINK

static FILE mydata;

// Types de mesure pour le bouton de selection (Ajout de MES_DISTANCE)
typedef enum {
	MES_TEMPERATURE = 0,
	MES_HUMIDITE,
	MES_PRESSION_ATM,
	MES_DISTANCE,
	NB_MESURES
} TypeMesure;

// Fonction d'initialisation de l'ADC pour lire le capteur Sharp sur PC2 (ADC2)
void initADC(void) {
	// Référence AVCC avec condensateur externe sur AREF (REFS1=0, REFS0=1)
	// Sélection du canal ADC2 (MUX[3:0] = 0010)
	ADMUX = (1 << REFS0) | (2 & 0x07);
	
	// Activer l'ADC et régler le prescaler à 64 (8MHz / 64 = 125kHz, idéal pour l'ADC)
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
}

// Fonction pour lire la valeur brute de l'ADC sur PC2
uint16_t readADC(void) {
	// S'assurer que le canal ADC2 est bien sélectionné
	ADMUX = (ADMUX & 0xF8) | (2 & 0x07);
	
	// Lancer une conversion
	ADCSRA |= (1 << ADSC);
	
	// Attendre la fin de la conversion
	while (ADCSRA & (1 << ADSC));
	
	return ADC;
}

// Fonction pour convertir la valeur ADC du GP2Y0A02YK en distance approximative (en cm)
// Note: Le capteur a une réponse non-linéaire (1/x). Formule empirique de base ou table de correspondance recommandée.
uint16_t convertADCtoDistance(uint16_t adc_val) {
	if (adc_val < 80) return 150; // Hors de portée max (~150cm)
	// Formule d'approximation standard pour GP2Y0A02YK (à ajuster selon vos mesures réelles)
	// Distance (cm) ? 27.86 / (Tension en V) ou approximation basée sur l'ADC
	float voltage = (float)adc_val * (5.0 / 1023.0);
	if (voltage < 0.4) return 150;
	uint16_t distance = (uint16_t)(65.0 / voltage); // Approximation empirique
	if (distance > 150) distance = 150;
	if (distance < 20) distance = 20;
	return distance;
}

//fonctions pour le port  RS232 (UART)
void initUSART(void) {
	UBRR0H = 0;              /* baud rate  */
	UBRR0L = 103;            /* 9600 */
	UCSR0A |= (1 << U2X0);     /* mode asynchrone double vitesse */
	UCSR0B |= (1 << TXEN0) | (1 << RXEN0);    /* Activer emission et reception  USART */
	UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);   /* 8 data bits, 1 stop bit */
}

void transmitByte(uint8_t data) {
	while ( !( UCSR0A & (1<<UDRE0)) );
	UDR0 = data;
}

uint8_t receiveByte(void) {
	while ( !( UCSR0A & (1<<RXC0)) );
	return UDR0;
}

void printString(const char myString[]) {
	uint8_t i = 0;
	while (myString[i]) {
		transmitByte(myString[i]);
		i++;
	}
}

//Programme principal
struct bme280_dev dev;

int main(void)
{
	// orienter printf vers l'afficheur TFT
	mydata.put = ili9341_putchar_printf;
	mydata.get = NULL;
	mydata.flags = _FDEV_SETUP_WRITE;
	stdout = &mydata;

	ili9341_init();
	ili9341_clear(BLACK);
	_delay_ms(1000);
	ili9341_setRotation(3);
	_delay_ms(2);
	
	display_init1();
	
	// Configuration du bouton-poussoir sur PC3 en entree avec pull-up
	DDRC &= ~(1 << DDC3);
	PORTC |= (1 << PORTC3);

	// Configuration de PC2 (ADC2) en entrée sans pull-up (analogique)
	DDRC &= ~(1 << DDC2);
	PORTC &= ~(1 << PORTC2);

	// Initialiser l'ADC pour le capteur de distance
	initADC();

	// Initialiser l'UART pour l'ESP8266 (9600 bauds)
	initUSART();
	
	// -------- Inits --------- //
	ili9341_settextcolour(CYAN,BLACK);
	_delay_ms(2);
	ili9341_settextsize(4);
	_delay_ms(2);
	
	// Affichage initial "LES CAPTEURS"
	ili9341_setcursor(4,160);
	ili9341_settextcolour(YELLOW,BLACK);
	printf("LES CAPTEURS ");
	ili9341_setcursor(4,200);
	ili9341_settextcolour(YELLOW,BLACK);
	printf("             ");
	_delay_ms(1500);
	
	int8_t bme_status = bme280_interface_selection(&dev, BME280_I2C_INTF);
	
	TypeMesure modeActif = MES_TEMPERATURE;
	uint8_t last_button_state = 1;
	bool need_title_redraw = true;
	uint16_t refresh_timer = 0;
	
	while (1) {
		// Lecture du bouton sur PC3
		uint8_t current_button_state = (PINC & (1 << PINC3)) ? 1 : 0;
		
		// Detection d'un front descendant
		if (last_button_state == 1 && current_button_state == 0) {
			_delay_ms(20); // Anti-rebond
			if (!(PINC & (1 << PINC3))) {
				modeActif = (TypeMesure)((modeActif + 1) % NB_MESURES);
				need_title_redraw = true;
				refresh_timer = 25;
			}
		}
		last_button_state = current_button_state;
		
		// Mise a jour de l'affichage
		if (need_title_redraw || refresh_timer >= 25) {
			refresh_timer = 0;
			
			if (bme_status != BME280_OK) {
				bme_status = bme280_interface_selection(&dev, BME280_I2C_INTF);
			}
			
			int32_t temp_int = 0, temp_dec = 0;
			uint32_t hum_int = 0, hum_dec = 0;
			uint32_t press_int = 0, press_dec = 0;
			int8_t read_status = BME280_E_DEV_NOT_FOUND;
			
			if (bme_status == BME280_OK) {
				read_status = bme280_read_data(&dev, &temp_int, &temp_dec, &hum_int, &hum_dec, &press_int, &press_dec);
			}

			// Lecture de la distance (GP2Y0A02YK)
			uint16_t adc_val = readADC();
			uint16_t distance_cm = convertADCtoDistance(adc_val);
			
			switch (modeActif) {
				case MES_TEMPERATURE:
				if (need_title_redraw) {
					ili9341_setcursor(4,160);
					ili9341_settextcolour(PINK,BLACK);
					printf(" TEMPERATURE ");
				}
				ili9341_setcursor(4,200);
				ili9341_settextcolour(PINK,BLACK);
				if (read_status == BME280_OK) {
					printf("  %2ld.%02ld C    ", (long)temp_int, (long)temp_dec);
					} else {
					printf("    ERREUR    ");
				}
				break;
				
				case MES_HUMIDITE:
				if (need_title_redraw) {
					ili9341_setcursor(4,160);
					ili9341_settextcolour(RED,BLACK);
					printf("  HUMIDITE   ");
				}
				ili9341_setcursor(4,200);
				ili9341_settextcolour(RED,BLACK);
				if (read_status == BME280_OK) {
					printf("  %2lu.%02lu %%    ", (unsigned long)hum_int, (unsigned long)hum_dec);
					} else {
					printf("    ERREUR    ");
				}
				break;
				
				case MES_PRESSION_ATM:
				if (need_title_redraw) {
					ili9341_setcursor(4,160);
					ili9341_settextcolour(GREEN,BLACK);
					printf("  PRESSION   ");
				}
				ili9341_setcursor(4,200);
				ili9341_settextcolour(GREEN,BLACK);
				if (read_status == BME280_OK) {
					printf(" %4lu.%02lu hPa ", (unsigned long)press_int, (unsigned long)press_dec);
					} else {
					printf("    ERREUR    ");
				}
				break;

				case MES_DISTANCE:
				if (need_title_redraw) {
					ili9341_setcursor(4,160);
					ili9341_settextcolour(ORANGE,BLACK);
					printf("  DISTANCE   ");
				}
				ili9341_setcursor(4,200);
				ili9341_settextcolour(ORANGE,BLACK);
				printf("   %3u cm    ", distance_cm);
				break;
				
				default:
				break;
			}
			
			// Envoi de toutes les 4 mesures par UART vers l'ESP8266 (format JSON)
			char uart_buf[80];
			snprintf(uart_buf, sizeof(uart_buf),
				"{\"temp\":%ld.%02ld,\"hum\":%lu.%02lu,\"press\":%lu.%02lu,\"dist\":%u}\r\n",
				(long)temp_int, (long)temp_dec,
				(unsigned long)hum_int, (unsigned long)hum_dec,
				(unsigned long)press_int, (unsigned long)press_dec,
				distance_cm);
			printString(uart_buf);

			need_title_redraw = false;
		}
		
		_delay_ms(20);
		refresh_timer++;
	}
}