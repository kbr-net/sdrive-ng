#include <avr/io.h>
#include <avr/interrupt.h>
#include "avrlibtypes.h"
#include "avrlibdefs.h"
#include "global.h"

u08 blinktimes;
u08 blinkleds;

ISR(TIMER1_OVF_vect) {
	static u08 s = 0;

	if (s) {
		LED_PORT &= ~blinkleds;	// LEDs an
		s = 0;
	}
	else {
		LED_PORT |= blinkleds;	// LEDs aus
		if (blinktimes == 0)
			blinkleds &= ~_BV(LED_RED_PIN);
		else
			blinktimes--;
		if (!blinkleds) {
			TCCR1B = 0;	// Timer 1 stop
			return;
		}
		s = 1;
	}
}

void blink_off (u08 leds) {
	blinkleds &= ~leds;
}

void blink_on (u08 leds, u08 times) {
	//if (times) blinktimes = times*2-1;
	if (times) blinktimes = times-1;
	blinkleds |= leds;

	// das reicht dann vermutlich einmal global, tut aber nicht weh
#if defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__)
	TIMSK1 |= _BV(TOIE1);		// Timer overflow interrupt enable
#else
	TIMSK |= _BV(TOIE1);		// Timer overflow interrupt enable
#endif
	// das kann man sich sparen, wenn es eh immer 0 ist
	// ne, ist ganz gut, wenn ein weiterer blink dazwischen kommt
	//TCNT1H = 0;			// Timer Preload setzen, H first
	TCNT1 = 0;			// Timer Preload setzen

	//LED_PORT &= ~_BV(LED_RED_PIN);	// LED an
	LED_PORT &= ~blinkleds;	// LEDs an
#if defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__)
	GTCCR |= _BV(PSRSYNC);		// Prescaler reset
#else
	SFIOR |= _BV(PSR10);		// Prescaler reset
#endif
	TCCR1B = _BV(CS11) | _BV(CS10);	// Timer 1 clk/64 start
					// 14,318MHz/64/64k = 3,41Hz
	sei();
}

void blink(u08 times) {
	blink_on(_BV(LED_RED_PIN), times);
}
