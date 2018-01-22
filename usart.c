//*****************************************************************************
// usart.c
// Bob!k & Raster, C.P.U., 2008
// Ehanced by kbr, 2014
//*****************************************************************************

#include <avr/io.h>		// include I/O definitions (port names, pin names, etc)
#include <util/delay.h>
#include "usart.h"
#include "global.h"

#include "avrlibdefs.h"		// global AVRLIB defines
#include "avrlibtypes.h"	// global AVRLIB types definitions

#if defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__)
	#define UCSRA	UCSR0A
	#define UCSRB	UCSR0B
	#define UCSRC	UCSR0C
	#define UDRE	UDRE0
	#define RXEN	RXEN0
	#define TXEN	TXEN0
	#define URSEL	0		// does not exist
	#define UCSZ0	UCSZ00
	#define UCSZ1	UCSZ01
	#define U2X	U2X0
	#define RXC	RXC0
	#define UDR	UDR0
#endif

extern unsigned char atari_sector_buffer[256];
extern unsigned char last_key;

unsigned char get_checksum (unsigned char* buffer, u16 len) {
	u16 i;
	u08 sumo,sum;
	sum=sumo=0;
	for(i=0;i<len;i++)
	{
		sum+=buffer[i];
		if(sum<sumo) sum++;
		sumo = sum;
	}
	return sum;
}

void USART_Init ( u08 value ) {
	/* Wait for empty transmit buffer */
	while ( !( UCSRA & (1<<UDRE)) ); //cekani

	/* Set baud rate */
#if defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__)
	UBRR0 = value;
#else
	UBRRH = 0;	//HB =0
	UBRRL = value;
#endif

	/* Set double speed flag */
	//	UCSRA = (1<<UDRE)|(1<<U2X); //double speed
	UCSRA = (1<<UDRE);

	/* Enable Receiver and Transmitter */
	UCSRB = (1<<RXEN)|(1<<TXEN);
	/* Set frame format: 8data, 1stop bit */
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);
	{
	 //unsigned char dummy;
	 while ( UCSRA & (1<<RXC) ) inb(UDR);
	}
}

void USART_Transmit_Byte( unsigned char data ) {
	/* Wait for empty transmit buffer */
	while ( !( UCSRA & (1<<UDRE)) )	;

	/* Put data into buffer, sends the data */
	UDR = data;
}

unsigned char USART_Receive_Byte( void ) {
	/* Wait for data to be received */
	while ( !(UCSRA & (1<<RXC)) );

	/* Get and return received data from buffer */
	return UDR;
}

void USART_Send_Buffer(unsigned char *buff, u16 len) {
	while(len>0) { USART_Transmit_Byte(*buff++); len--; }
}

u08 USART_Get_Buffer_And_Check(unsigned char *buff, u16 len, u08 cmd_state) {
	//prebere len bytu + 1 byte checksum
	//pokud se behem nacitani zmeni cmd_state => return 0x01
	//pokud se behem nacitacni zmeni getkey => return 0x02
	//pak zkontroluje checksum
	//pokud nesouhlasi checksum, return 0x80
	//pokud vse ok, return 0x00
	u08 *ptr;
	u16 n;
	u08 b;
	ptr=buff;
	n=len;
	while(1)
	{ 
		// Wait for data to be received
		do
		{
			//pokud by prisel command L nebo stisknuta klavesa, prerusi prijem
			if ( get_cmd_H()!=cmd_state ) return 0x01;
			if ( read_key()!=last_key ) return 0x02;
		} while ( !(UCSRA & (1<<RXC)) );
		// Get and return received data from buffer
		//return UDR;
		b=UDR;
		if (!n)
		{
			//v b je checksum (n+1 byte)
			if ( b!=get_checksum(buff,len) ) return 0x80;	//chyba checksumu
			return 0x00; //ok
		}
		*ptr++=b;
		n--;
	}
}

u08 USART_Get_buffer_and_check_and_send_ACK_or_NACK(unsigned char *buff, u16 len) {
	unsigned char err;
	//tady pred ctenim zadna pauza

	err = USART_Get_Buffer_And_Check(buff,len,CMD_STATE_H);
	//vraci 0 kdyz ok, !=0 kdyz chyba

	_delay_ms(1);	//t4
	if(err)
	{
		send_NACK();
		return 0xff; //kdyz nesouhlasi checksum
	}
	send_ACK();
//	Delay300us(); //po ACKu je pauza (250usec az 255s), protoze se pak posila CMPL nebo ERR
	// ^ tahle pauza se dela pred CMPL nebo ERR
	return 0;	//kdyz je ok vraci 0
}

void USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(unsigned short len) {
	u08 check_sum;

	//	Delay300us();	//po ACKu pred CMPL pauza 250us - 255sec
	//Kdyz bylo jen 300us tak nefungovalo
	_delay_us(800);	//t5
	send_CMPL();
	//	Delay1000us();	//S timhle to bylo doposud (a nefunguje pod Qmegem3 Ultraspeed vubec)
	//	Delay100us(); 		//Pokusne kratsi pauza
						//(tato fce se pouziva se i u read SpeedIndex 3F,
						// coz s delsi pauzou nefunguje (pod Qmegem3 fast)
	//Delay800us();	//t6
	_delay_us(200);	//<--pouziva se i u commandu 3F

	USART_Send_Buffer(atari_sector_buffer,len);
	check_sum = get_checksum(atari_sector_buffer,len);
	USART_Transmit_Byte(check_sum);
}
