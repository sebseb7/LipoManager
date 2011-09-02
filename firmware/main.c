#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "main.h"


void updateLTCstate(void);

uint8_t volatile currentState = 3; // bit 0: 1==enabled per key , bit 1: 1==battery voltage sufficient
uint8_t volatile enableADC = 1;
uint8_t volatile downCount = 0;
uint8_t volatile restartDelay = 0;


#define Vuvlo 350 // +/-  ~ 0,15V
#define R1 680
#define R2 220
// this cast is very important, otherwise gcc optimizes this to 0
#define VADC Vuvlo*(uint32_t)R2/(R1+R2) // 85.5
#define ADCuvlo (VADC*1024)/110

#define UVLO_UPPER ADCuvlo+32 // 824 == 0,885V ADC Pin == 3,62V Bat
#define UVLO_LOWER ADCuvlo-32 // 759 == 0,815V ADC Pin == 3,33V Bat


//in switchless mode uvlo detection is allways running
//#define SWITCHLESS
//#define SWITCHLESS_FULLOFF

//todo: implement this mode
//#define ONE_TOGGLE_SWITCH

// PB1/INT0 connected to switch 1 (off)
// PB2/PCINT2 connected to switch 2 (on)
// PB3/ADC3 connected to voltage divider
// PB0 connected to LTC 3558 EN2


ISR(ADC_vect)
{
	if(ADC > UVLO_UPPER)
	{
		currentState |= 2;
	}

	if(ADC < UVLO_LOWER)
	{
#ifdef SWITCHLESS
		currentState &= ~2;
#else
		// set key state also to 0
		currentState = 0;
#endif
	}
	updateLTCstate();

	//disable ADC again
	ADCSRA &= ~(1<<ADEN);
	PRR |= (1<<PRADC);
	
}

//every second (every 4 seconds in switchless)
ISR(TIM0_OVF_vect)
{
	enableADC = 1;
#if defined(SWITCHLESS_FULLOFF) || !defined(SWITCHLESS)
	if((PINB & (1<<PINB2))==0)
	{
		downCount++;
		if(downCount == 15)
		{
			downCount=0;
			currentState &= ~1;
			updateLTCstate();
		}	
	}
	else
	{
		downCount=0;
	}
#endif	
	if(restartDelay > 0)
	{
		restartDelay--;
	}
}


#if defined(SWITCHLESS_FULLOFF) || !defined(SWITCHLESS)
//on
ISR(PCINT0_vect)
{
	// after each wake up from deep sleep, we also do ADC
	if(restartDelay == 0)
	{
		currentState |= 1;
		enableADC=1;
		updateLTCstate();
	}
}
#endif

#ifndef SWITCHLESS
//off
ISR(INT0_vect)
{
	currentState &= ~1;
	updateLTCstate();
}
#endif


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
		if(PORTB & (1<<PORTB0))
		{
			restartDelay = 3;
			PORTB &= ~(1<<PORTB0);
		}
	}
}


int main (void)
{
	
	//enable sw1 and sw2 pull-ups 
	PORTB |= (1<<PORTB2)|(1<<PORTB1);

#ifdef SWITCHLESS
	// set clock to 500Hz :-)
	CLKPR = (1<<CLKPCE);
	CLKPR = (1<<CLKPS3);

	// disable input buffers for switch1 and switch2 
	DIDR0 |= (1<<AIN1D);
#ifndef SWITCHLESS_FULLOFF
	DIDR0 |= (1>>ADC1D);
#endif
	currentState |= 1;

#else
	//enable interrupt for switch1 (off)
	MCUCR |= (1<<ISC00);
	GIMSK |= (1<<INT0);	
#endif

#if defined(SWITCHLESS_FULLOFF) || !defined(SWITCHLESS)
	//enable interrupt for switch2 (on)
	PCMSK |= (1<<PCINT2);
	GIMSK |= (1<<PCIE);	
#endif

	//use interal voltage reference and use ADC3
	ADMUX |= (1<<REFS0)|(1<<MUX1)|(1<<MUX0);
	//enable ADC interrupt
	ADCSRA |= (1<<ADIE);
	//disable digital input buffer for the ADC3 Pin to reduce noise for ADC
	DIDR0 |= (1<<ADC3D);
	//disable digital input buffer for the ADC2/PB4 the pin is not connected and consumes additional 260uA otherwise (alternativly one could enable the internal pull-up)
	PORTB |= (1<<PORTB4);
	DIDR0 |= (1<<ADC2D); 
	//disable the analog comparator
	ACSR |= (1<<ACD);


#ifdef SWITCHLESS
	//enable timer0 (prescaler 8)
	TCCR0B |= (1<<CS01); // (clock is 500, == ~.24 TIM0_OVF interrupts per second)
#else
	//enable timer0 (prescaler 64)
	TCCR0B |= (1<<CS01)|(1<<CS00); // (clock is 16k, == ~1 TIM0_OVF interrupts per second)
#endif
	//enable timer0 overflow interrupt
	TIMSK0 |= (1<<TOIE0);
	
	// globally enable sleep
	MCUCR |= (1<<SE);
	
	//globally enable interrupts
	sei();

	updateLTCstate();



	// current consuption when fully activated : 4,5mA (should be slightly less) 

	while(1)
	{
		MCUCR &= ~((1<<SM0)|(1<<SM1));
		if(enableADC == 1)
		{
			enableADC=0;
			// set sleep mode to ADC
			MCUCR |= (1<<SM0);
			//enable ADC
			PRR &= ~(1<<PRADC);
			ADCSRA |= (1<<ADEN);
		}
		else
		{
#ifndef SWITCHLESS
			// go into powerdown mode if not fully activated (consumtion :  ~5uA )
			if(currentState != 3)
			{
				if(restartDelay == 0)
				{
					MCUCR |= (1<<SM1);
				}
			}
#else
			// in switchless mode we go into idle sleep (consumtion : ~75uA )

#ifdef SWITCHLESS_FULLOFF
			// in switchless_fulloff mode we go into powerdown sleep (consumtion : ~5uA )
			// reset is neccessary to reactivate it
			if((currentState & 2) == 0)
			{
				if(restartDelay == 0)
				{
					MCUCR |= (1<<SM1);
				}
			}
#endif
#endif

		}

		asm volatile("sleep");
	}
}


