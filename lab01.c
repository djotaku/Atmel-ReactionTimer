/* (C) 2005 by
 * Richard West (richard@dopplereffect.us)
 * Eric Mesa (djotaku1282@yahoo.com)
 * Lab 01 - Reaction Time Tester
 * Lab Date: 2/9/05
 * Published: 22 Feb 2005
 * Published under GNU General Public License - http://www.linux.org/info/gnu.html
 *
 * Hardware setup notes:
 *		Atmel Mega32 running on STK500
 *		LEDs connected to PortB.0-7
 *		16x2 LCD connected to PortC.0-7
 *		pushbutton2 connected to PortD.2(Int0)
 *		pushbutton7 connected to PortD.7
 *
 * Optional features:
 *		counter overflow detection (ie, dead contestant dection)
 *		improved cheat handling (sometimes quirky)
 *		fastest time in eeprom
 */

// Mega32 compiler directive
#include <Mega32.h>

// LCD compiler directives
#asm(".equ __lcd_port=0x15")
#include <lcd.h>

// useful C library
#include <stdio.h> // for sprintf

// operating states
#define FLASH 0
#define DARK 1
#define TEST 2
#define DONE 3

// lcd_flag states
#define NONE 0
#define START 1
#define CHEAT 2
#define TIME 3
#define SLOW 4

// go_flag states
#define RUNNING 0
#define RESTART 1

// bounce time for pushbuttons
#define BOUNCETIME 30

// runtime global variables
unsigned int count; // time in ms
unsigned int delay; // psuedorandom delay time in ms
unsigned int randcount; // psuedorandom number  
unsigned int reacttime; // reaction time in ms
eeprom unsigned int fastest = 0xFFFF; // fastest time in ms
unsigned char mode; // operation/state mode
unsigned char go_flag; // operation flag
unsigned char lcd_flag; // LCD control flag
unsigned char lcd_buffer[16]; // LCD buffer

// function prototypes
void init(void);
void start(void);
void lcd_display_start(void);
void lcd_display_cheat(void);
void lcd_display_time(void);
void lcd_display_slow(void);
void lcd_display_fastest(void);

// timer0 compare interrupt service routine
interrupt [TIM0_COMP] void tim0_comp_isr(void)
{
	if (mode == FLASH)
	{
		if (count > 500) // period = 500 ms; 1 Hz flashing
		{
			PORTB ^= 0xFF; // toggle LEDs
			count = 0; // reset count
		}
		else
		{
			count++; // increment count
		}
	}
	else if (mode == DARK)
	{
		if (count > delay) // delay period over
		{
			PORTB = 0x00; // turn on LEDs
			count = 0; // reset count

			if (PIND & 0x04) // if button2 is not pushed (on D2)
			{
				mode = TEST; // switch to TEST mode
			}
			else
			{
				// set lcd_flag for cheater message
				lcd_flag = CHEAT;

				// switch to DONE mode
				mode = DONE;
			}
		}
		else
		{
			count++; // increment count
		}
	}
	else if (mode == TEST)
	{
		count++; // increment count
		
		/*
		Optional feature:
		detects overflow of counter
		*/
		if (count == 0xFFFF)
		{
			// set lcd_flag for too slow message
			lcd_flag = SLOW;

			// switch to DONE mode			
			mode = DONE;
		}
	}
	else if (mode == DONE)
	{
		if (!(PIND & 0x80)) // if button7 is pushed (on D7)
		{
			// set restart flag;
			go_flag = RESTART;
		}
	}
}

// external interrupt0 interrupt service routine
interrupt [EXT_INT0] void ext_int0_isr(void)
{
	if (mode == FLASH)
	{
		PORTB = 0xFF; // turn off the LEDs
		count = 0; // reset count

		// delay is a psuedorandom number between 1.5 and 2.5 seconds
		delay = 1500 + (randcount%1000);

		mode = DARK; // switch to DARK mode
	}
	/*
	optional feature under developement,
	and it occationally has some quirks
	*/
	else if (mode == DARK)
	{
		// allow button to stop bouncing
		// but catch any attempted bounce cheating
		if (count > BOUNCETIME)
		{
			// set lcd_flag for cheater message
			lcd_flag = CHEAT;

			// switch to DONE mode
			mode = DONE;
		}
	}
	else if (mode == TEST)
	{
		// store reaction time
		reacttime = count;
		
		/*
		Optional feature:
		records the fastest time
		*/
		if (reacttime < fastest)
		{
			fastest = reacttime;
		}

		// set lcd_flag for reaction time
		lcd_flag = TIME;

		// switch to DONE mode
		mode = DONE;
	}
}

void main(void)
{
	lcd_init(16); // initialize LCD (LCD must be hooked up for execution to proceed)
	init(); // initialize MCU
	start(); // initialize gameplay

	while(1)
	{
		// psuedorandom counter
		randcount++;

		// check if game should restart
		if (go_flag == RESTART)
		{
			start();
		}

		// LCD messages done inside main() for interruptability
		// check if any LCD flags are set
		if (lcd_flag == START)
		{
			// display opening message on LCD
			lcd_display_start();
		}
		else if (lcd_flag == CHEAT)
		{
			// display cheater message on LCD
			lcd_display_cheat();
		}
		else if (lcd_flag == TIME)
		{
			// display reaction time on LCD
			lcd_display_time();
		}
		else if (lcd_flag == SLOW)
		{
			// display too slow on LCD
			lcd_display_slow();
		}
		lcd_flag = NONE; // clear lcd_flag
	}
}

void init(void)
{
	// initialize ports
	DDRB = 0xFF; // set portB as output
	DDRD = 0x00; // set portD as input
	PORTB = 0xFF; // turn off all LEDs

	// setup timer0
	// 16 MHz system clock
	// Prescaler = 64 -> 250 kHz timer
	TIMSK = 0x02; // enable the timer0 compare interrupt
	OCR0 = 0xF9; // set compare register to 249 (250 ticks = 1ms)
	TCCR0 = 0x0B; // set prescaler and clear-on-match

	// pushbutton2 setup
	GICR = 0x40; // enable external interrupt (int0)
	MCUCR = 0x02; // interrupt on falling edge trigger

	// enable interrupts
	#asm("sei")
}

void start(void)
{
	count = 0; // reset count
	/*
	// "no need to throw away entropy" ~ G. Roth '05
	randcount = 0; // reset random counter
	*/
	go_flag = RUNNING; // set go_flag to running
	lcd_flag = START; // set lcd_flag for opening message

	mode = FLASH; // start in FLASH mode

	PORTB = 0x00; // turn on LEDs
}

void lcd_display_start(void)
{
	// setup lcd_buffer
	sprintf(lcd_buffer,"%u",fastest);
	
	// display opening message on LCD
	/******************
	 *How fast are you*
	 *<fastest>    ms?*
	 ******************/
	lcd_clear();
	lcd_gotoxy(0,0);
	lcd_putsf("How fast are you");
	lcd_gotoxy(0,1);
	lcd_puts(lcd_buffer);
	lcd_gotoxy(13,1);
	lcd_putsf("ms?");
}

void lcd_display_cheat(void)
{
	// display cheater message on LCD
	/******************
	 *!!! CHEATER !!! *
	 *                *
	 ******************/
	lcd_clear();
	lcd_gotoxy(0,0);
	lcd_putsf("!!! CHEATER !!!");
}

void lcd_display_time(void)
{
	// setup lcd_buffer
	sprintf(lcd_buffer,"%u",reacttime);

	// display reaction time on LCD
	/******************
	 *Reaction time:  *
	 *<time>        ms*
	 ******************/
	lcd_clear();
	lcd_gotoxy(0,0);
	lcd_putsf("Reaction time:");
	lcd_gotoxy(0,1);
	lcd_puts(lcd_buffer);
	lcd_gotoxy(14,1);
	lcd_putsf("ms");
}

void lcd_display_slow(void)
{
	// display too slow on LCD
	/******************
	 * !! TOO SLOW !! *
	 *                *
	 ******************/
	lcd_clear();
	lcd_gotoxy(1,0);
	lcd_putsf("!! TOO SLOW !!");
}
