/*-----------------------------------------------------------------------*/
/* Stripped down version of:						 */
/* MMCv3/SDv1/SDv2 (in SPI mode) control module  (C)ChaN, 2007           */
/*-----------------------------------------------------------------------*/

#include <avr/io.h>
#include "mmc.h"
#include "config.h"


/* SD card SPI access */

#define SPI_PORT_SCK   	SPI_PORT
#define SPI_DDR_SCK    	SPI_DDR

#define SPI_PORT_MISO  	SPI_PORT
#define SPI_DDR_MISO   	SPI_DDR

#define SPI_PORT_MOSI  	SPI_PORT
#define SPI_DDR_MOSI   	SPI_DDR

#define SAMEPORT(a,b) ((x##a) == (x##b))

/* SPI macros */

#define SPI_SET_SPEED_F_2	do {SPCR = (1<<SPE) | (1<<MSTR) | (0<<SPR1) | (0<<SPR0); SPSR = (1<<SPI2X); } while(0)
#define SPI_SET_SPEED_F_4	do {SPCR = (1<<SPE) | (1<<MSTR) | (0<<SPR1) | (0<<SPR0); SPSR = (0<<SPI2X); } while(0)
#define SPI_SET_SPEED_F_8	do {SPCR = (1<<SPE) | (1<<MSTR) | (0<<SPR1) | (1<<SPR0); SPSR = (1<<SPI2X); } while(0)
#define SPI_SET_SPEED_F_16	do {SPCR = (1<<SPE) | (1<<MSTR) | (0<<SPR1) | (1<<SPR0); SPSR = (0<<SPI2X); } while(0)
#define SPI_SET_SPEED_F_32	do {SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR1) | (0<<SPR0); SPSR = (1<<SPI2X); } while(0)
#define SPI_SET_SPEED_F_64	do {SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR1) | (0<<SPR0); SPSR = (0<<SPI2X); } while(0)
#define SPI_SET_SPEED_F_128	do {SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR1) | (1<<SPR0); SPSR = (0<<SPI2X); } while(0)

/* switch to fast SPI Clock */
#define SPISetFastClock()	SPI_SET_SPEED_F_2
#define SPISetSlowClock()	SPI_SET_SPEED_F_8

#if F_CPU/128 > 100000
 #define SPISetMMCInitClock()	SPI_SET_SPEED_F_128
#elif F_CPU/64 > 100000
 #define SPISetMMCInitClock()	SPI_SET_SPEED_F_64
#else
 #define SPISetMMCInitClock()	SPI_SET_SPEED_F_32
#endif

/* Port Controls */
#define CS_LOW()	SD_CS_PORT &= ~(1<<SD_CS_PIN)	/* MMC CS = L */
#define CS_HIGH()	SD_CS_PORT |=  (1<<SD_CS_PIN)	/* MMC CS = H */

#define	FCLK_SLOW()	SPISetMMCInitClock()	/* Set slow clock (100k-400k) */
#define	FCLK_FAST()	SPISetFastClock()	/* Set fast clock (depends on the CSD) */


/* Card type flags (cardtype) */
#define CT_MMC				0x01	/* MMC ver 3 */
#define CT_SD1				0x02	/* SD ver 1 */
#define CT_SD2				0x04	/* SD ver 2 */
#define CT_SDC				(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK			0x08	/* Block addressing */

/* Definitions for MMC/SDC command */
#define CMD0	(0)		/* GO_IDLE_STATE */
#define CMD1	(1)		/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)		/* SEND_IF_COND */
#define CMD9	(9)		/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */


extern uint8_t fat_buf[];

/*--------------------------------------------------------------------------

 Module Private Functions

 ---------------------------------------------------------------------------*/

static
uint8_t cardtype; 			/* Card type flags */

static void spi_wait(void) {
	loop_until_bit_is_set(SPSR,SPIF);
}

static void spi_write(uint8_t a) {
	SPDR = a;
}

static void spi_xmit(uint8_t a){
	spi_write(a);
	spi_wait();
}

static uint8_t spi_rcvr(void) {
	spi_write(0xff);
	spi_wait();
	return SPDR;
}

/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static 
uint8_t wait_ready (void)	/* 1:Ready, 0:Timeout */
{
	uint16_t i = 0x0000;

	spi_rcvr();
#if 1
	while (--i && (spi_rcvr() != 0xFF))
		;
	return  i != 0;
#else
	do
		if (spi_rcvr() == 0xFF)
		{
			return 1;
		}
	while (--i);

	return 0;
#endif
}

/*-----------------------------------------------------------------------*/
/* Deselect the card and release SPI bus                                 */
/*-----------------------------------------------------------------------*/

static
void deselect (void)
{
	CS_HIGH();
	spi_rcvr();
}

/*-----------------------------------------------------------------------*/
/* Select the card and wait for ready                                    */
/*-----------------------------------------------------------------------*/

static
uint8_t select(void)	/* 1:Successful, 0:Timeout */
{
	CS_LOW();
	if (wait_ready()) {
		return 1;
	}
	deselect();
	return 0;
}


/*-----------------------------------------------------------------------*/
/* Receive a data packet from MMC                                        */
/*-----------------------------------------------------------------------*/

static
uint8_t rcvr_datablock (void) 
//uint8_t rcvr_datablock (uint8_t *buff) 
{
	uint8_t token;
	uint16_t waitcount = 0x0000;

	do {
		token = spi_rcvr();
	} while ((token == 0xFF) && --waitcount);

	if(token != 0xFE) return 0;		/* If not valid data token, retutn with error */

	for (int i = 0; i < MMC_SECTOR_SIZE; i++)
		fat_buf[i] = spi_rcvr();
//		*buff++ = spi_rcvr();
	spi_rcvr(); 				/* Discard CRC */
	spi_rcvr();

	return 1; /* Return with success */
}


static
uint8_t send_cmd (uint8_t cmd, uint32_t arg) __attribute__ ((noinline));

static
uint8_t send_cmd_0 (uint8_t cmd) __attribute__ ((noinline));

static
uint8_t send_cmd_0 (uint8_t cmd)
{
	return send_cmd(cmd, 0);
}


/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/
/* Returns R1 response (bit7==1: Send failed) */

static
uint8_t send_cmd (uint8_t cmd, uint32_t arg)
{
	uint8_t n, res;

	if (cmd & 0x80) { /* ACMD<n> is the command sequence of CMD55-CMD<n> */
		cmd &= 0x7F;
		res = send_cmd_0(CMD55);
		if (res > 1)
			return res;
	}

	/* Select the card and wait for ready */
	deselect();
	if (!select()) return 0xFF;

	/* Send command packet */
	spi_xmit(0x40 | cmd); /* Start + Command index */
#if 0
	spi_xmit((uint8_t)((arg & 0xFF000000) >> 24));
	spi_xmit((uint8_t)((arg & 0x00FF0000) >> 16));
	spi_xmit((uint8_t)((arg & 0x0000FF00) >>  8));
	spi_xmit((uint8_t) (arg & 0x000000FF));
#else
	union {
		uint32_t as32;
		uint8_t as8[4];
	} argtmp;

	argtmp.as32 = arg;
	spi_xmit(argtmp.as8[3]); /* Argument[31..24] */
	spi_xmit(argtmp.as8[2]); /* Argument[23..16] */
	spi_xmit(argtmp.as8[1]); /* Argument[15..8] */
	spi_xmit(argtmp.as8[0]); /* Argument[7..0] */
#endif

	if (cmd == CMD0)
		n = 0x95; 	/* Valid CRC for CMD0(0) */
	else if (cmd == CMD8)
		n = 0x87; 	/* Valid CRC for CMD8(0x1AA) */
	else
		n = 0x01; 	/* Dummy CRC + Stop */

	spi_xmit(n);

	n = 10; /* Wait for a valid response in timeout of 10 attempts */
	do
		res = spi_rcvr();
	while ((res & 0x80) && --n);

	return res; /* Return with the response value */
}

/*--------------------------------------------------------------------------

 Public Functions

 ---------------------------------------------------------------------------*/

#if 1

#undef CT_SD1
#undef CT_MMC
#define CT_SD1 CT_SD2
#define CT_MMC CT_SD2

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/
/* return 0 = ok, 1 = error, not ok, etc.                                */

uint8_t mmc_init (void)
{
	uint8_t n, cmd, ty;
	uint16_t waitcount;


#if SAMEPORT(SD_CS_PORT,SPI_PORT)
	SD_CS_PORT |= _BV(SD_CS_PIN);
	SPI_DDR    |= _BV(SD_CS_PIN) | _BV(SPI_MOSI) | _BV(SPI_SCK);
#else
	SD_CS_PORT |= _BV(SD_CS_PIN);
	SD_CS_DDR  |= _BV(SD_CS_PIN); 	// Turns on CS pin as output
	SPI_DDR    |= _BV(SPI_MOSI) | _BV(SPI_SCK);	// also SCK
#endif
	SPI_PORT   |= _BV(SPI_SCK);	// set SCK high
	SPI_PORT   |= _BV(SPI_MISO);	// turn on pullup on MISO

	FCLK_SLOW();

	for (n = 10; n; --n)
		spi_rcvr(); /* 80 dummy clocks */

	ty = 0;
	waitcount = 0x0000;							/* Initialization timeout of ?000 msec */
	if (send_cmd_0(CMD0) == 1) { 						/* Enter Idle state */
		if (send_cmd(CMD8, 0x1AA) == 1) {				/* SDv2? */
			for (n = 4; n; --n) 
				spi_rcvr();					/* Get trailing return value of R7 response */
			while (--waitcount && send_cmd(ACMD41, 1UL << 30))
				;						/* Wait for leaving idle state (ACMD41 with HCS bit) */
			if (waitcount && send_cmd_0(CMD58) == 0) {		/* Check CCS bit in the OCR */
				ty = spi_rcvr();
				for (n = 3; n; --n) 
					spi_rcvr();				/* Get trailing return value of R7 response */
				ty = (ty & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 */
			}
		} else {					/* SDv1 or MMCv3 */
			if (send_cmd_0(ACMD41) <= 1) {
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			while (--waitcount && send_cmd_0(cmd));		/* Wait for leaving idle state */
			if (!waitcount || send_cmd(CMD16, 512) != 0)	/* Set R/W block length to 512 */
				ty = 0;
		}
	}
	cardtype = ty;
	deselect();

	if (ty) { /* Initialization succeded */
		FCLK_FAST();
	} 

	return ty == 0;
}

#else

 
/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/
/* return 0 = ok, 1 = error, not ok, etc.                                */

uint8_t mmc_init (void)
{

	/*static*/ uint8_t ocr[4];
	uint8_t n, cmd, ty;
	uint16_t waitcount;

#ifdef CS_ON_SPI_PORT
	SD_CS_PORT |= _BV(SD_CS_PIN);
	SPI_DDR    |= _BV(SD_CS_PIN) | _BV(SPI_MOSI) | _BV(SPI_SCK);
#else
	SD_CS_PORT |= _BV(SD_CS_PIN);
	SD_CS_DDR  |= _BV(SD_CS_PIN); // Turns on CS pin as output
	SPI_DDR    |= _BV(SPI_MOSI) | _BV(SPI_SCK);
#endif
	FCLK_SLOW();

	for (n = 10; n; n--)
		spi_rcvr(); /* 80 dummy clocks */

	ty = 0;
	waitcount = 0x0000;							/* Initialization timeout of ?000 msec */
	if (send_cmd_0(CMD0) == 1) { 						/* Enter Idle state */
		if (send_cmd(CMD8, 0x1AA) == 1) {				/* SDv2? */
			for (n = 0; n < 4; n++) ocr[n] = spi_rcvr();		/* Get trailing return value of R7 response */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) { 		/* The card can work at vdd range of 2.7-3.6V */
				while (--waitcount && send_cmd(ACMD41, 1UL << 30));	/* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (waitcount && send_cmd_0(CMD58) == 0) {		/* Check CCS bit in the OCR */

					for (n = 0; n < 4; n++) ocr[n] = spi_rcvr();
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 */

					ty = spi_rcvr();
					spi_rcvr(); spi_rcvr(); spi_rcvr();
					ty = (ty & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 */
				}
			}
		} else {					/* SDv1 or MMCv3 */
			if (send_cmd_0(ACMD41) <= 1) {
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			while (--waitcount && send_cmd_0(cmd));		/* Wait for leaving idle state */
			if (!waitcount || send_cmd(CMD16, 512) != 0)	/* Set R/W block length to 512 */
				ty = 0;
		}
	}
	cardtype = ty;
	deselect();

	if (ty) { /* Initialization succeded */
		FCLK_FAST();
	} else { /* Initialization failed */
		select();				/* Wait for card ready */
		deselect();
	}

	return ty == 0;
}
#endif

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
/* Start sector number (LBA) */

//uint8_t mmc_read (uint8_t *buff, uint32_t sector)
uint8_t mmc_read (uint32_t sector)
{
	uint8_t result;

	if (!(cardtype & CT_BLOCK)) 
		sector *= 512;	/* Convert to byte address if needed */

//	if ((send_cmd(CMD17, sector) == 0) && rcvr_datablock(buff))
	if ((send_cmd(CMD17, sector) == 0) && rcvr_datablock())
		result = RES_OK;
	else
		result = RES_ERROR;

	deselect();

	return result;
}

