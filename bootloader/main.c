#include <avr/io.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <util/crc16.h>
#include <util/delay.h>
#include "fat.h"
#include "config.h"


#ifndef USE_FLASH_LED
 #define USE_FLASH_LED 0
#endif
#if USE_FLASH_LED
 #define FLASH_LED_ON()	 FLASH_LED_PORT = FLASH_LED_POLARITY ? FLASH_LED_PORT & ~(1<<FLASH_LED_PIN) \
							     : FLASH_LED_PORT |  (1<<FLASH_LED_PIN)
 #define FLASH_LED_OFF() FLASH_LED_PORT = FLASH_LED_POLARITY ? FLASH_LED_PORT |  (1<<FLASH_LED_PIN) \
							     : FLASH_LED_PORT & ~(1<<FLASH_LED_PIN)
 #define FLASH_LED_TOGGLE() FLASH_LED_PORT ^=  (1<<FLASH_LED_PIN)

 #define EE_LED_ON()		EE_LED_PORT &= ~(1<<EE_LED_PIN)
 #define EE_LED_OFF()		EE_LED_PORT |= (1<<EE_LED_PIN)
 #define EE_LED_TOGGLE()	EE_LED_PORT ^= (1<<EE_LED_PIN)
#else
 #define FLASH_LED_ON()
 #define FLASH_LED_OFF()
 #define FLASH_LED_TOGGLE()

 #define EE_LED_ON()
 #define EE_LED_OFF()
 #define EE_LED_TOGGLE()
#endif


//#define APPSIZE (FLASHEND - BOOTLDRSIZE + 1)
#define APPSIZE (FLASHEND - BOOTLDRSIZE + 1 + E2END + 1)
#define FLASHSIZE (FLASHEND - BOOTLDRSIZE + 1)
#define INFOSIZE (sizeof(bootldrinfo_t))

typedef struct {
	uint32_t dev_id;
	uint16_t app_version;
	uint16_t crc;
} bootldrinfo_t;

#ifdef USE_FAT32
static uint32_t startcluster;
static uint32_t updatecluster = 0; //is set when update is available
#else
static uint16_t startcluster;
static uint16_t updatecluster = 0; //is set when update is available
#endif
static bootldrinfo_t current_bootldrinfo;


static inline uint16_t crc_file(void) {
	uint16_t filesector;
	uint16_t flash_crc = 0xFFFF;

	for (filesector = 0; filesector < APPSIZE / 512; filesector++) {
		FLASH_LED_TOGGLE();

		fat_readfilesector(startcluster, filesector);

		for (int index = 0; index < 512; index++) {
			flash_crc = _crc_ccitt_update(flash_crc, fat_buf[index]);
		}
	}
	FLASH_LED_ON();

	return flash_crc; // result is ZERO when CRC is okay
}

static inline void check_file(void) {

	bootldrinfo_t *file_bootldrinfo;

	//Check filesize
	
	//if (filesize != FLASHEND - BOOTLDRSIZE + 1)
	if (filesize != APPSIZE)
		return;

	//Get last sector
	fat_readfilesector(startcluster, APPSIZE / 512 - 1);
	file_bootldrinfo =  (bootldrinfo_t*) (uint8_t*) (fat_buf + 512 - INFOSIZE);
	
	//Check DEVID
	if (file_bootldrinfo->dev_id != DEVID)
		return;

	EE_LED_ON();
		
	//Check application version
	if (file_bootldrinfo->app_version != 0 &&
	    file_bootldrinfo->app_version <= current_bootldrinfo.app_version && 
	    current_bootldrinfo.app_version != 0)
		return;
	
	// If development version in flash and in file, check for different crc
	if (current_bootldrinfo.app_version == 0 &&
	    file_bootldrinfo->app_version   == 0 &&
	    current_bootldrinfo.crc == file_bootldrinfo->crc)
	        return;

	// check CRC of file
	if(crc_file() != 0)
		return;
			
	current_bootldrinfo.app_version = file_bootldrinfo->app_version;
	updatecluster = startcluster;
}

static inline void flash_update(void) {

	uint16_t *lpword;
#if (FLASHEND > 0xFFFF)
	uint32_t adr;
#else
	uint16_t adr;
#endif

	unsigned char *eptr = 0;
	adr = 0;

	for (int filesector = 0; filesector < APPSIZE / 512; filesector++) {

		lpword = (uint16_t*) fat_buf;
		fat_readfilesector(updatecluster, filesector);
		
		if (filesector < FLASHSIZE / 512) {	//flash
			FLASH_LED_TOGGLE();
			for (unsigned int i = 0; i < (512/SPM_PAGESIZE); i++, adr += SPM_PAGESIZE) {
				
				boot_page_erase(adr);
				boot_spm_busy_wait();
		
				for (unsigned int j=0; j<SPM_PAGESIZE; j+=2)
					boot_page_fill(adr + j, *lpword++);

				boot_page_write(adr);
				boot_spm_busy_wait();
			}
		}
		else {					//eeprom
			for (unsigned int i = 0; i < sizeof(fat_buf); i++) {
				if (i%8 == 0)
					EE_LED_TOGGLE();
				eeprom_update_byte(eptr++, fat_buf[i]);
			}
			//// better bytewise, then we can flash the led
			//eeprom_update_block(fat_buf, eptr, sizeof(fat_buf));
			//eptr += sizeof(fat_buf);	//next block
		}
	}
	while (boot_rww_busy())
		boot_rww_enable();
}

void flash_check (void) {
	uint16_t flash_crc = 0xFFFF;

	// flash
	for (int index = 0; index < FLASHSIZE; index++) {
		flash_crc = _crc_ccitt_update(flash_crc, pgm_read_byte((char*) index));
	}
	// eeprom
	for (int index = 0; index <= E2END; index++) {
		flash_crc = _crc_ccitt_update(flash_crc, eeprom_read_byte((uint8_t *) index));
	}

	while (flash_crc) {		// stay here on error and toggle
		FLASH_LED_ON();
		EE_LED_OFF();
		_delay_ms(500);
		FLASH_LED_OFF();
		EE_LED_ON();
		_delay_ms(500);
	}
}


void main(void) __attribute__ ((noreturn));
void main(void) {

	static void (*app_start)(void) = 0x0000;
	uint8_t rc;

	/* Disable watchdog timer */
#ifdef WDTCSR
	WDTCSR |= (1<<WDCE) | (1<<WDE);
	WDTCSR  = 0x00;
#elif defined WDTCR
 #ifndef WDCE
  #ifdef WDTOE
   #define WDCE WDTOE
  #endif
 #endif
 #ifdef WDCE
	WDTCR |= (1<<WDCE) | (1<<WDE);
	WDTCR  = 0x00;
 #else
	/* don't know, how to handle watchdog */
 #endif
#endif

#if USE_FLASH_LED
	FLASH_LED_ON();
	FLASH_LED_DDR |= (1<<FLASH_LED_PIN);
	EE_LED_DDR |= (1<<EE_LED_PIN);
	EE_LED_OFF();
#endif


/*
#if FLASHEND > 0xFFFF
	memcpy_PF(&current_bootldrinfo, (uint_farptr_t)APPSIZE - INFOSIZE, INFOSIZE);
#else
	memcpy_P (&current_bootldrinfo,    (uint8_t *) APPSIZE - INFOSIZE, INFOSIZE);
#endif
*/

	eeprom_read_block(&current_bootldrinfo, (char *) E2END - INFOSIZE + 1, INFOSIZE);

	if (current_bootldrinfo.app_version == 0xFFFF) {
			current_bootldrinfo.app_version = 0; //application not flashed yet
	}
	
	rc = fat_init();
	if (rc == 0) {
		for (uint16_t i = 0; i < 512; i++) {
			startcluster = fat_readRootDirEntry(i);

			//if (startcluster == 0xFFFF)
			if (startcluster == -1)
				continue;

			check_file();
		}

		if (updatecluster) {
			flash_update();
			flash_check();
		}
	}

#if USE_FLASH_LED
	FLASH_LED_OFF();
	FLASH_LED_DDR &= ~(1<<FLASH_LED_PIN);
#endif
	app_start();
	for (;;);
}
