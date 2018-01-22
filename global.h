#define LED_PORT		PORTC
#define LED_DDR			DDRC
#define LED_PIN			PINC
#define LED_GREEN_PIN		5
#define LED_RED_PIN		4
// Aktivita
#define LED_GREEN_ON		cbi(LED_PORT,LED_GREEN_PIN)
#define LED_GREEN_OFF		sbi(LED_PORT,LED_GREEN_PIN)
// Zapis
#define LED_RED_ON		cbi(LED_PORT,LED_RED_PIN)
#define LED_RED_OFF		sbi(LED_PORT,LED_RED_PIN)

#define get_readonly()          ( inb(PIND) & (1<<PIND3) )      //read only
#define read_key()              (inb(PIND)&0xe4)

