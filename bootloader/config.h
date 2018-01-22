#ifndef CONFIG_H
#define CONFIG_H 1

#include <avr/io.h>

/* SD card socket connections */

#define SPI_PORT      	PORTB		/* SPI Connection port */
#define SPI_DDR       	DDRB		/* SPI Direction port */
#define SPI_PIN       	PINB		/* SPI Pin port */

#ifdef __AVR_ATmega32__
	#define SPI_SCK		7
	#define SPI_MISO	6
	#define SPI_MOSI	5
	#define SD_CS_PIN	4
#else	//ATmega328...
	#define SPI_SCK		5
	#define SPI_MISO	4
	#define SPI_MOSI	3
	#define SD_CS_PIN	2
#endif
#define SD_CS_PORT	PORTB		/* CS (Card Select) pin */
#define SD_CS_DDR	DDRB


#define USE_FLASH_LED		1	/* 0 = no LED used */
#define FLASH_LED_PORT		PORTC
#define FLASH_LED_DDR 		DDRC
#define FLASH_LED_PIN 		4
#define FLASH_LED_POLARITY	1

#define EE_LED_PORT		PORTC
#define EE_LED_DDR 		DDRC
#define EE_LED_PIN 		5
#define EE_LED_POLARITY		1

#endif /* CONFIG_H */

