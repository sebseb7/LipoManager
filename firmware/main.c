#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "main.h"


void updateLTCstate(void);

uint8_t volatile currentState = 0; // bit 0: 1==enabled per key , bit 1: 1==battery voltage sufficient


// PB3/ADC3 connected to voltage divider
// PB2/PCINT2 connected to switch 1
// PB3/PCINT1 connected to switch 2 
// PB0 connected to LTC 3558 EN2


ISR(ADC_vect)
{
	// TODO: do ADC readout 
	
	
	// if > 3.8V
	currentState |= 2;
	
	// if < 3.5V
	// currentSate &= ~2;
	
	
	updateLTCstate();
	
}

ISR(TIM0_OVF_vect)
{
	// TODO: start ADC here
}


ISR(PCINT0_vect)
{
	if( (PINB & (1<<PINB2)) == 0)
	{
		currentState |= 1;
	}

	if( (PINB & (1<<PINB3)) == 0 )
	{
		currentState &= ~1;
	}

	updateLTCstate();

}

int main (void)
{

	//enable interrupt for switch1 and switch2
	PCMSK |= (1<<PCINT2)|(1<<PCINT1);
	GIMSK |= (1<<PCIE);	
	

	//set sw1 and sw2 as input and enable pull-ups 
	DDRB &= ~((1<<PINB2)|((1<<PINB2)));
	PORTB |= (1<<PORTB2)|(1<<PORTB3);

	//enable timer0 (prescaler 1024)
	TCCR0B |= (1<<CS02)|(1<<CS00); // (clock is 600k, so 2,29 TIM0_OVF interrupts per second
	//eable timer0 overflow interrupt
	TIMSK0 |= (1<<TOIE0);
	

	while(1)
	{
	}
}

void updateLTCstate(void)
{

	if(currentState == 3)
	{
		// enable LTC EN2
		PORTB |= (1<<PORTB0);
	}
	else
	{
		// disable LTC EN2
		PORTB &= ~(1<<PORTB0);
	}


	// TODO: go into sleep here


}
