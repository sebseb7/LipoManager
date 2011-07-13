#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include "main.h"


void updateLTCstate(void);

uint8_t volatile currentState = 0; // bit 0: 1==enabled per key , bit 1: 1==battery voltage sufficient
uint8_t volatile enableADC = 1;
uint8_t volatile timer_count = 0;

// upper and lower undervoltage locker values are stored in eeprom
uint16_t uvlo_upper_ee EEMEM = 850;
uint16_t uvlo_lower_ee EEMEM = 800;
uint16_t uvlo_upper = 0;
uint16_t uvlo_lower = 0;

// PB1/INT0 connected to switch 1 (off)
// PB2/PCINT2 connected to switch 2 (on)
// PB3/ADC3 connected to voltage divider
// PB0 connected to LTC 3558 EN2


ISR(ADC_vect)
{

	uint16_t adc_res = ADC;

	if(adc_res > uvlo_upper)
	{
		currentState |= 2;
	}

	if(adc_res < uvlo_lower)
	{
		currentState &= ~2;
	}
	updateLTCstate();


	//disable ADC again
	ADCSRA &= ~(1<<ADEN);
	PRR |= (1<<PRADC);
	enableADC=0;
	
}

ISR(TIM0_OVF_vect)
{
	timer_count++;

	//enable adc every ~20 seconds
	if(timer_count > 50)
	{
		enableADC = 1;
		timer_count=0;
	}
}

ISR(PCINT0_vect)
{
	currentState |= 1;
	updateLTCstate();
}

ISR(INT0_vect)
{
	currentState &= ~1;
	updateLTCstate();
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
}


int main (void)
{
	uvlo_upper = eeprom_read_word(&uvlo_upper_ee);
	uvlo_lower = eeprom_read_word(&uvlo_lower_ee);

	if(uvlo_upper == 0xFFFF) uvlo_upper = 850;
	if(uvlo_lower == 0xFFFF) uvlo_lower = 800;

	//enable sw1 and sw2 pull-ups 
	PORTB |= (1<<PORTB2)|(1<<PORTB1);

	//enable interrupt for switch1
	PCMSK |= (1<<PCINT2);
	GIMSK |= (1<<PCIE);	


	//enable interrupt for switch2
	MCUCR |= (1<<ISC00);
	GIMSK |= (1<<INT0);	
	
	//use interal voltage reference and use ADC3
	ADMUX |= (1<<REFS0)|(1<<MUX1)|(1<<MUX0);
	//enable ADC interrupt
	ADCSRA |= (1<<ADIE);
	//disable digital input buffer for the ADC3 Pin
	DIDR0 |= (1<<ADC3D);


	//enable timer0 (prescaler 1024)
	TCCR0B |= (1<<CS02)|(1<<CS00); // (clock is 600k, so 2,29 TIM0_OVF interrupts per second
	//enable timer0 overflow interrupt
	TIMSK0 |= (1<<TOIE0);
	
	// globally enable sleep
	MCUCR |= (1<<SE);
	
	//globally enable interrupts
	sei();

	updateLTCstate();

	while(1)
	{
		MCUCR &= ~((1<<SM0)|(1<<SM1));
		if(enableADC == 1)
		{
			// set sleep mode to ADC
			MCUCR |= (1<<SM0);
			//enable ADC
			PRR &= ~(1<<PRADC);
			ADCSRA |= (1<<ADEN);
		}
		asm volatile("sleep");
	}
}


