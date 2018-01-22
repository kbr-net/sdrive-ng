#include <avr/io.h>
#include <util/delay.h>
#include "fat.h"		/* fat_buf[] */
#include "mmc_lib.h"


static unsigned char cmd[6];

#define MMC_ENABLE  MMC_PORT &= ~(1<<MMC_CS)	//MMC Chip Select -> Low (activate)
#define MMC_DISABLE MMC_PORT |= 1<<MMC_CS; //MMC Chip Select -> High (deactivate);

static inline void spi_send_byte(uint8_t data)
{
	SPDR=data;
	loop_until_bit_is_set(SPSR, SPIF); // wait for byte transmitted...
}

static uint8_t send_cmd(void)
{
	uint8_t i;
	
	spi_send_byte(0xFF); //Dummy delay 8 clocks
	
	MMC_ENABLE;
	
	_delay_loop_2(1500);

/*
	for (i=0; i<6; i++)
	{//Send 6 Bytes
		spi_send_byte(cmd[i]);
	}
*/

        i=0;
	while(i<6)
	{//Send 6 Bytes
		spi_send_byte(cmd[i]);
		i++;
	}

	uint8_t result;
	
	for(i=0; i<128; i++)
	{//waiting for response (!0xff)
		spi_send_byte(0xFF);
		
		result = SPDR;
		
		if ((result & 0x80) == 0)
			break;
	}
	
	return(result); // TimeOut !
}

uint8_t mmc_init(void)
{
	SPCR = 0;
	//MMC_DDR = 1<<SPI_CLK | 1<<SPI_MOSI | 1<<MMC_CS;	//MMC Chip Select -> Output
	//MMC_DDR = 0;
	MMC_DDR = 1<<SPI_CLK | 1<<SPI_MOSI | 1<<MMC_CS;	//MMC Chip Select -> Output
	
	SPCR = 1<<SPE | 1<<MSTR | SPI_INIT_CLOCK; //SPI Enable, SPI Master Mode
	SPSR = 0;

	uint8_t i;

	i = (uint8_t)12;
	while (i)
	{//Pulse 80+ clocks to reset MMC
		spi_send_byte((uint8_t)0xFF);
		i--;
	}
	
	uint8_t res;

	cmd[0] = (uint8_t)0x40 + MMC_GO_IDLE_STATE;
	cmd[1] = 0x00; 
	cmd[2] = 0x00; 
	cmd[3] = 0x00; 
	cmd[4] = 0x00; 
	cmd[5] = 0x95;
//	cmd[5] = 0x95;
	
	for (i=0; i<MMC_CMD0_RETRIES; i++)
	{
		res = send_cmd(); //store result of reset command, should be 0x01
		
		MMC_DISABLE;

		spi_send_byte((uint8_t)0xFF);
		if (res == (uint8_t)0x01)
			break;
	}
	
	if (i == MMC_CMD0_RETRIES)
		return MMC_CMD0_TIMEOUT;
	
	if (res != (uint8_t)0x01) //Response R1 from MMC (0x01: IDLE, The card is in idle state and running the initializing process.)
		return(MMC_INIT);
	
	cmd[0] = (uint8_t)0x40 + MMC_SEND_OP_COND;
	
	i = (uint8_t)255;
	
	while (i)
	{
		if (send_cmd() == (uint8_t)0)
		{
			MMC_DISABLE;
			spi_send_byte((uint8_t)0xFF);
			spi_send_byte((uint8_t)0xFF);
			return MMC_OK;
		}
		i--;
	}
	
	MMC_DISABLE;
	
	return(MMC_OP_COND_TIMEOUT);
}

static inline uint8_t wait_start_byte(void)
{
	uint16_t i = 0;
	
	do
	{
		spi_send_byte((uint8_t)0xFF);
		if (SPDR == (uint8_t)0xFE)
			return MMC_OK;
		
		i++;
	} while (i);
	
	return MMC_NOSTARTBYTE;
}

uint8_t mmc_read(uint32_t adr)
{
	adr <<= 1;
	
	cmd[0] = (uint8_t)(0x40 + MMC_READ_SINGLE_BLOCK);
	cmd[1] = (uint8_t)((adr & 0x00FF0000) >> 0x10);
	cmd[2] = (uint8_t)((adr & 0x0000FF00) >> 0x08);
	cmd[3] = (uint8_t)(adr & 0x000000FF);
	cmd[4] = (uint8_t)0;
	
	SPCR = 1<<SPE | 1<<MSTR | SPI_READ_CLOCK; //SPI Enable, SPI Master Mode
	
	if (send_cmd())
	{
		MMC_DISABLE;
		return(MMC_CMDERROR); //wrong response!
	}
	
	if (wait_start_byte())
	{
		MMC_DISABLE;
		return MMC_NOSTARTBYTE;
	}
	
//	return(MMC_OK);
//}
//
//void mmc_read_buffer(void)
//{
	unsigned char *buf = fat_buf;
	while (buf < &fat_buf[MMC_SECTOR_SIZE])
	{
		spi_send_byte((uint8_t)0xFF);
		*(buf++) = SPDR;
	}
//}
//
//void mmc_stop_read_block(void)
//{
	//read 2 bytes CRC (not used);
	spi_send_byte((uint8_t)0xFF);
	spi_send_byte((uint8_t)0xFF);
	MMC_DISABLE;
	return(MMC_OK);
}
