/*
 * Copyright (C) 2025 Radiofan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ANALOG_H_
#define ANALOG_H_
 
#include <avr/io.h>

// General bit setting macros
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif
#ifndef checkBit
#define checkBit(reg,bit) (reg&(1<<bit)) 
#endif

#define ADC_Close() ADCSRA = 0x00;

#define PRESCALE_2		1
#define PRESCALE_4		2
#define PRESCALE_8		3
#define PRESCALE_16		4
#define PRESCALE_32		5	
#define PRESCALE_64		6
#define PRESCALE_128	7

void inline ADC_Init(uint8_t PRESCALE)
{
  // Enable the ADC peripheral
  sbi(ADCSRA,ADEN);
  // Least significant three bits are the prescale settings
  ADCSRA |= PRESCALE;
}

void inline ADC_Start(uint8_t ch) {
  // Set the desired channel
  ADMUX = ch; 
  // Start conversion
  sbi(ADCSRA,ADSC);
}

uint16_t inline ADC_Result() {
  uint16_t value;
  // Wait for conversion
  while(checkBit(ADCSRA,ADSC));
  // Read the result
  value = ADCL; value += ADCH << 8; 
  return value;
}

uint16_t inline ADC_Read(uint8_t ch)
{
  ADC_Start(ch);
  return ADC_Result() >> 2;
}

#endif
