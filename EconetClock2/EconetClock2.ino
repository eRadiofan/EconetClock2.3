/************************************************************************
	main.c

    Acorn Econet Clock 2
    Copyright (C) 2017,2025 Simon Inns, Radiofan

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Email: simon.inns@gmail.com, eradiofan@gmail.com

************************************************************************/

/* Arduino IDE 2.x settings:
 *
 * Board: ATTinyCore/ATtiny45 (No bootloader)
 * BOD: Disabled
 * Clock Source: 8MHz (internal)
 * Save EEPROM: EEPROM retained
 * millis()/micros(): Disabled
 * Timer 1 clock: CPU
 */

// Global includes
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "Analog.h"

/* DIP switch 1 toggles between a a mark/space ratio of 1/3 (off) and 1/5 (on).
 * A short mark will allow more time for the data to settle,
 *   but if it is too short, the clock signal might become unreliable.
 * DIP switches 2-4 adjust the clock fequency in a binary coded manner.
 * The frequencies are approximately:
 *   100KHz, 200KHz, 250KHz, 300KHz, 350KHz, 400KHz, 450KHz, 500KHz
 */

// ---------------------------------------------------------------------------------
// Hardware definitions

// Clock out positive (PB3 - !OC1B)
#define CLOCKOUTP_PORT	PORTB
#define CLOCKOUTP_PIN	PINB
#define CLOCKOUTP_DDR	DDRB
#define CLOCKOUTP		(1 << 3)

// Clock out negative (PB4 - OC1B)
#define CLOCKOUTN_PORT	PORTB
#define CLOCKOUTN_PIN	PINB
#define CLOCKOUTN_DDR	DDRB
#define CLOCKOUTN		(1 << 4)

// DIP Switch 1 (LSB)
#define DIPSW1_PORT	PORTB
#define DIPSW1_PIN	PINB
#define DIPSW1_DDR	DDRB
#define DIPSW1		(1 << 2)

// DIP Switch 2
#define DIPSW2_PORT	PORTB
#define DIPSW2_PIN	PINB
#define DIPSW2_DDR	DDRB
#define DIPSW2		(1 << 1)

// DIP Switch 3 (MSB)
#define DIPSW3_PORT	PORTB
#define DIPSW3_PIN	PINB
#define DIPSW3_DDR	DDRB
#define DIPSW3		(1 << 0)

typedef struct {
  uint8_t period;
  uint8_t mark;
} timing;

#define mark_third(x) (x-(x/3))
#define mark_fifth(x) (x-(x/5))

// These are 16MHz counts, stored in flash memory
static const timing timings[16] PROGMEM = {
	//                    Period (uS) Mark (us) Frequency
	{160, mark_third(160)}, // 10.0		3.313     100KHz
	{160, mark_fifth(160)}, // 10.0   2.000
	{80, mark_third(80)},   // 5.00		1.625     200KHz
	{80, mark_fifth(80)},   // 5.00		1.000
	{64, mark_third(64)},   // 4.00		1.313     250KHz
	{64, mark_fifth(64)},   // 4.00		0.750
	{53, mark_third(53)},   // 3.31		1.063     302KHz
	{53, mark_fifth(53)},   // 3.31		0.625
	{46, mark_third(46)},   // 2.88		0.938     348KHz
	{46, mark_fifth(46)},   // 2.88   0.563
	{40, mark_third(40)},   // 2.50   0.813     400KHz
	{40, mark_fifth(40)},   // 2.50   0.500
	{36, mark_third(36)},   // 2.25   0.750     444KHz
	{36, mark_fifth(36)},   // 2.25   0.438
	{32, mark_third(32)},   // 2.00   0.625     500KHz
	{32, mark_fifth(32)}    // 2.00   0.375
};

int main(void) {
	// Variable to store the current and previous DIP switch settings (0-7)
	int8_t currentDipSwitch = 0;
	int8_t previousDipSwitch = -1;

	// Set the CPU clock prescaler (overrides CKDIV8 fuse if set)
	CLKPR=(1 << CLKPCE);
	CLKPR=(0 << CLKPCE) | (0 << CLKPS3) | (0 << CLKPS2) | (0 << CLKPS1) | (0 << CLKPS0);

	// Configure the clock out negative pin
	CLOCKOUTN_DDR |= CLOCKOUTN;			  // Set direction to output
	CLOCKOUTN_PORT &= ~(CLOCKOUTN);		// Set output to off

	// Configure the clock out positive pin
	CLOCKOUTP_DDR |= CLOCKOUTP;			  // Set direction to output
	CLOCKOUTP_PORT &= ~(CLOCKOUTP);		// Set output to off

	// Set DIP Switch pins to input and turn on weak pull-ups
	DIPSW1_DDR &= ~DIPSW1;
	DIPSW2_DDR &= ~DIPSW2;
	DIPSW3_DDR &= ~DIPSW3;

	DIPSW1_PORT |= DIPSW1;
	DIPSW2_PORT |= DIPSW2;
	DIPSW3_PORT |= DIPSW3;

	// Positive (non-inverted) PWM output on OC1B (PB4)
	// Negative (inverted) PWM output on !OC1B (PB3)

	// Initialize Timer/Counter1 for asynchronous mode:
	// To set Timer/Counter1 in asynchronous mode first enable PLL
	// and then wait 100 uS for PLL to stabilize. Next, poll
	// the PLOCK bit until it is set and then set the PCKE bit.
	
	// Enable PLL and divide 64MHz by 2
	PLLCSR |= ((1 << PLLE) | (1 << LSM));

	// Wait 100uS for the PLL to stabilize
	_delay_ms(100);

	// Wait for the PLOCK bit to be set in PLLCSR
	while ((PLLCSR & (1 << PLOCK)) == 0x00);
	
	// Enable the PCK clock for asynchronous mode
	// (i.e. route it to timer/counter1)
	PLLCSR |= (1 << PCKE);
	
	// We will use Timer/Counter1 in PWM mode
	
	// Disconnect OC1A as we don't need it
	TCCR1 |= (0 << COM1A1) | (0 << COM1A0);
	TCCR1 |= (1 << PWM1A); // do we need this?
	
	// Connect OC1B and its inverted friend !OC1B
	//
	// We want COM1B1 = 0 and COM1B0 = 1:
	// OC1B cleared on compare match. Set when TCNT1 = &00
	// OC1B set on compare match. Cleared when TCNT1 = &00
	GTCCR |= (0 << COM1B1) | (1 << COM1B0);
	
	// Now we have to enable the clock and set the prescaler value
	// (prescale is the amount the clock is divided before being
	// used as an input to Timer/Counter1).  The faster the clock
	// ticks, the more accurately we can time the period and mark
	// needed for the Econet clock, but the overall period is then
	// less (since the timer is 8-bit and can only span a maximum
	// of 255 ticks).
	
	// Enable Timer/Counter1 in Asynchronous mode with PCK/2 prescaler (=16MHz)
	TCCR1 |= (0 << CS13) | (0 << CS12) | (1 << CS11) | (0 << CS10);
	
	// Enable Pulse Width Modulator B
	GTCCR |= (1 << PWM1B);

	// Initialise the ADC in order to read the 4th bit
	ADC_Init(PRESCALE_2);

  // Monitor the DIP switch settings and set the clock when it changes.
  while (1) {
		// Read the current DIP switch setting
		currentDipSwitch = 0;
		if (ADC_Read(0) < 213) currentDipSwitch += 8;
		if (!(DIPSW1_PIN & DIPSW1)) currentDipSwitch += 1;
		if (!(DIPSW2_PIN & DIPSW2)) currentDipSwitch += 2;
		if (!(DIPSW3_PIN & DIPSW3)) currentDipSwitch += 4;

		// Has the DIP switch setting changed?
		if (currentDipSwitch != previousDipSwitch) {
			// If so, adjust the clock.
			// Mark is set by OCR1B and the overall period is set by OCR1C
			OCR1B = pgm_read_byte(&(timings[currentDipSwitch].mark));
			OCR1C = pgm_read_byte(&(timings[currentDipSwitch].period));
			previousDipSwitch = currentDipSwitch;
		}
	}
}

