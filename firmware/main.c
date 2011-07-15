#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include "main.h"


void updateLTCstate(void);

uint8_t volatile currentState = 0; // bit 0: 1==enabled per key , bit 1: 1==battery voltage sufficient
uint8_t volatile enableADC = 1;

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

	if(enableADC == 2)
	{
		// set uvlo values
		uvlo_upper = adc_res + 25;
		uvlo_lower = adc_res - 25;
		
		eeprom_write_word(&uvlo_upper_ee,  uvlo_upper);
		eeprom_write_word(&uvlo_lower_ee,  uvlo_lower);
	}
	else
	{

		if(adc_res > uvlo_upper)
		{
			currentState |= 2;
		}

		if(adc_res < uvlo_lower)
		{
			// set set key state also to 0
			currentState = 0;
		}
		updateLTCstate();

	}


	//disable ADC again
	ADCSRA &= ~(1<<ADEN);
	PRR |= (1<<PRADC);
	enableADC=0;
	
}

//every 16 seconds
ISR(TIM0_OVF_vect)
{
	enableADC = 1;
}


//on
ISR(PCINT0_vect)
{
	// after each wake up from deep sleep, we also do ADC
	enableADC = 1;
	currentState |= 1;
	updateLTCstate();
}

//off
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

	
	
	//check for sw2 (on) for setting uvlo values
	if( ((PINB >> PINB2) & 1)==0 )
	{
		// 2 == store uvlo
		enableADC = 2;
	}
	

	//enable interrupt for switch1 (off)
	MCUCR |= (1<<ISC00);
	GIMSK |= (1<<INT0);	

	//enable interrupt for switch2 (on)
	PCMSK |= (1<<PCINT2);
	GIMSK |= (1<<PCIE);	
	
	//use interal voltage reference and use ADC3
	ADMUX |= (1<<REFS0)|(1<<MUX1)|(1<<MUX0);
	//enable ADC interrupt
	ADCSRA |= (1<<ADIE);
	//disable digital input buffer for the ADC3 Pin to reduce noise for ADC
	DIDR0 |= (1<<ADC3D);
	//disable digital input buffer for the ADC2/PB4 the pin is not connected and consumes additional 260µA otherwise (alternativly one could enable the internal pull-up)
	DIDR0 |= (1<<ADC2D); 


	//enable timer0 (prescaler 1024)
	TCCR0B |= (1<<CS01)|(1<<CS00); // (clock is 16k, 7,8 TIM0_OVF interrupts per second, no problem to be that fast, because we do this only when actived per key)
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
		if(enableADC > 0)
		{
			// set sleep mode to ADC
			MCUCR |= (1<<SM0);
			//enable ADC
			PRR &= ~(1<<PRADC);
			ADCSRA |= (1<<ADEN);
		}
		else
		{
			// go into powerdown mode if not fully activated (consumtion :  25-40µA )
			if(currentState != 3)
			{
				MCUCR |= (1<<SM1);
			}
			//else: if fully activated, we let the timer run and w also need idle mode to detect pinchange on int0 (does not work)
			// 		(consumtion 100-123µA, but in this mode the LTC is also enabled)
		}
		asm volatile("sleep");
	}
}


