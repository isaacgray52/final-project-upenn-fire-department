/*
 * upenn-fire-department.c
 *
 * Created: 4/12/2024 10:02:03 AM
 * Author : olive
 */ 

#define F_CPU 16000000UL
#define BAUD_RATE 9600
#define BAUD_PRESCALER (((F_CPU / (BAUD_RATE * 16UL))) - 1)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdio.h>

#include "uart.h"
#include "main.h"

char buf[80];

uint8_t outputCounter = 0;

bool trigSent = false;
bool trigOutputHigh = false;
bool echoStarted = false;
bool echoEnded = false;

uint8_t overflows = 0;
uint16_t echoStartTime = 0;
uint16_t echoEndTime = 0;

bool outputON = false;

uint8_t highTime = 200;
uint8_t lowTime = 100;

bool isTriggered = true;

uint8_t TWI_addr_to_read = 0;

TWI_state TWI_State = SEND_MESSAGE;

uint8_t TWI_Error = 0;

bool TWI_send_message = true;
bool TWI_send_SLA = false;
bool TWI_send_data_addr = false;
bool TWI_send_data_stop = false;
bool TWI_receive_data_start = false;
bool TWI_receive_data_send_SLA = false;
bool TWI_receive_data_sent_SLA = false;
bool TWI_receive_data_receive = false;
bool TWI_first_time = true;

uint8_t TWI_data_received;

uint16_t temperatureVals[64];
uint8_t temperatureCounter = 0;
uint8_t read_high = false;

uint32_t USavgVals[AVG_NUM_VALS];
uint32_t USavg = 0;

uint8_t avgCounter = 0;

uint32_t timeDifferenceUS(uint32_t startTime, uint32_t endTime, uint8_t overflowCount) {
	return ((endTime + (((uint32_t)overflowCount) * ((uint32_t)UINT16_MAX))) - startTime) / 2;
}

uint16_t counter = 0;


void initialize(void) {
	cli();

	for (uint8_t i = 0; i < 64; i++) {
		temperatureVals[i] = 0;
	}
	
	UART_init(BAUD_PRESCALER);

	// input for timer1 (input capture) (echo measuring)
	SET_ZERO(DDRB, DDRB0);
	// output for timer2 (trig send) (10us)
	SET_ONE(DDRB, DDRB3);
    // output for timer0 PWM for servo
    SET_ONE(DDRD, DDRD6);

	// set outputs for TWI
	SET_ONE(DDRC, DDRC4);
	SET_ONE(DDRC, DDRC5);
	
	/** TIMER 0 **/
	
	// Set fast PWM for timer 0
	SET_ONE(TCCR0B, WGM02);
	SET_ZERO(TCCR0A, WGM01);
	SET_ONE(TCCR0A, WGM00);
	
	// 256 - 61.2Hz
	// Set a timer 0 prescaler of 64
	// means a base (lowest) frequency of 245Hz with full timer compare to 
	SET_ZERO(TCCR0B, CS02);
	SET_ONE(TCCR0B, CS01);
	SET_ONE(TCCR0B, CS00);
	
	// Set overflow interrupt enable
	SET_ONE(TIMSK0, TOIE0);

    // Set OC0A on Compare Match, clear OC0A at BOTTOM (inverting mode)
    // SET_ONE(TCCR0A, COM0A1);
    // SET_ONE(TCCR0A, COM0A0);

    OCR0A = 200;

    // SET_ONE(PORTD, PORTD6);
	
	/** TIMER 1 **/
	
	// Set Normal mode
	SET_ZERO(TCCR1B, WGM13);
	SET_ZERO(TCCR1B, WGM12);
	SET_ZERO(TCCR1A, WGM11);
	SET_ZERO(TCCR1A, WGM10);
	
	// Set a prescaler of 8
	SET_ZERO(TCCR1B, CS12);
	SET_ONE(TCCR1B, CS11);
	SET_ZERO(TCCR1B, CS10);
	
	// Enable input capture noise canceler
	SET_ONE(TCCR1B, ICNC1);
	
	// Set input capture to capture positive/rising edge
	SET_ONE(TCCR1B, ICES1);
	
	// Enable input capture interrupt enable
	SET_ONE(TIMSK1, ICIE1);
	
	// Set overflow interrupt enable
	SET_ONE(TIMSK1, TOIE1);
	
	
	/** TIMER 2 **/
	
	// Set a timer 2 prescaler of 8 (2 MHz clock)
	SET_ZERO(TCCR2B, CS22);
	SET_ONE(TCCR2B, CS21);
	SET_ZERO(TCCR2B, CS20);
	
	// Set CTC mode for timer 2
	SET_ZERO(TCCR2B, WGM22);
	SET_ONE(TCCR2A, WGM21);
	SET_ZERO(TCCR2A, WGM20);
	
	// Enable Output compare A interrupt for timer 2
	SET_ONE(TIMSK2, OCIE2A);
	
	// Set OCRA for output A compare match interrupt for timer 2 (20 ticks = 10us)
	OCR2A = 30;

    /** TWI (I2C) **/
    // for communicating with thermal camera
	// Using TWI0

	SET_ZERO(PRR0, PRTWI0);

	// 200kHz scl clock frequency
	// SCL frequency = CPU Clock frequency / (16 + 2(TWBR) * PrescalerValue)

	// no prescaler
	SET_ZERO(TWSR0, TWPS0);
	SET_ZERO(TWSR0, TWPS1);

	TWBR0 = 32;

	// enable TWI
	SET_ONE(TWCR0, TWEN);

	// enable interrupt
	// SET_ONE(TWCR0, TWIE);
	
	// clear interrupt
	SET_ONE(TWCR0, TWINT);

    sei();
}


int main(void)
{
    initialize();
    /* Replace with your application code */
    while (1) 
    {
		if (TWI_State == SEND_MESSAGE) {
			if (!TWI_first_time) {
				if (temperatureCounter == 63 && read_high) {
					uint16_t temp_max = 0;
					uint8_t temp_max_idx = 0;
					for (uint8_t i = 0; i < 64; i++) {
						if (temperatureVals[i] > temp_max) {
							temp_max = temperatureVals[i];
							temp_max_idx = i;
						}
						// snprintf(buf, sizeof(buf), "%02hu: %u\n", i, temperatureVals[i]);
						// UART_putstring(buf);
					}
					snprintf(buf, sizeof(buf), "max_temp: %u\n", temp_max);
					UART_putstring(buf);
					snprintf(buf, sizeof(buf), "max_temp_idx: %hu\n", temp_max_idx);
					UART_putstring(buf);
					if (temp_max > FIRE_THRESHOLD) {
						UART_putstring("FIIIIIRREEE!!!!\n\n");
					}
					// while (!(TWCR0 & (1 << TWINT)));
				}
				if (read_high) {
					temperatureVals[temperatureCounter] = (temperatureVals[temperatureCounter] & 0x00FF) | ((TWI_data_received & 0x0F) << 8);
					temperatureCounter = (temperatureCounter + 1) % 64;
				} else {
					temperatureVals[temperatureCounter] = (temperatureVals[temperatureCounter] & 0xFF00) | TWI_data_received;
				}
				read_high = !read_high;
			}
			TWI_first_time = false;
			TWI_State = SEND_SLA;
			TWI_addr_to_read = IR_TEMP_REG + (temperatureCounter << 1) + (read_high ? 1 : 0);
			// snprintf(buf, sizeof(buf), "addr_to_read: %hX\n", TWI_addr_to_read);
			// UART_putstring(buf);
			TWCR0 = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE);//0b10100101;
		}
		if (counter == 1000) {
			// snprintf(buf, sizeof(buf), "dat2: %hu\n", TWI_data_received2);
			// UART_putstring(buf);
			// counter = 0;
			// if (isTriggered) {
			// 	highTime = 70;
			// 	lowTime = 35;
			// } else {
			// 	highTime = 50;
			// 	lowTime = 50;
			// }
			// isTriggered = !isTriggered;
		} else {
			counter++;
		}

    }
}


ISR(TIMER2_COMPA_vect) {
	if (!trigSent) {
		echoStarted = false;
		echoEnded = false;
		if (!trigOutputHigh) {
			SET_ONE(PORTB, PORTB3);
			trigOutputHigh = true;
		} else {
			SET_ZERO(PORTB, PORTB3);
			trigOutputHigh = false;
			trigSent = true;
		}
	}
}

ISR(TIMER1_CAPT_vect) {
	// flip checking for rising/falling edge
	// SET_FLIP(TCCR1B, ICES1);
	if (GET_BIT(PINB, PINB0)) {
		echoStartTime = ICR1;
		echoStarted = true;
		overflows = 0;
		// interrupt on falling edge
		SET_ZERO(TCCR1B, ICES1);
	} else {
		echoEndTime = ICR1;
		echoEnded = true;
		trigSent = false;
		// interrupt on rising edge
		SET_ONE(TCCR1B, ICES1);
		uint32_t diffUS = timeDifferenceUS(echoStartTime, echoEndTime, overflows);
		USavgVals[avgCounter] = diffUS;
		avgCounter = (avgCounter + 1) % AVG_NUM_VALS;
		USavg = 0;
		for (int i = 0; i < AVG_NUM_VALS; i++) {
	 		USavg += USavgVals[i];
		}
		USavg = USavg / AVG_NUM_VALS;
		outputCounter = (outputCounter + 1) % 100;
		
		uint16_t cm = USavg / 58;
		if (outputCounter == 0) {
			//snprintf(message, 100, "pressTime=%5u \tendTime=%5u \toverflows=%2u \tdiffUS=%5lu \tcm=%3lu \tin=%3lu\n", echoStartTime, echoEndTime, overflows, diffUS, diffUS / 58, diffUS / 148);
			snprintf(buf, sizeof(buf), "diffUS=%5lu \tcm=%3u\n", USavg, cm);
			UART_putstring(buf);
			if (cm > 10) {
				if (isTriggered) {
					highTime = 70;
					lowTime = 35;
				} else {
					highTime = 50;
					lowTime = 50;
				}
				isTriggered = !isTriggered;
			}
		}
		
	}
}

ISR(TIMER1_OVF_vect) {
	//UART_send('X');
	overflows++;
	if (trigSent && overflows > 5 && (!echoStarted)) {
		trigSent = false; // resend trig
		overflows = 0;
	}
}

ISR(TIMER0_OVF_vect) {
    if (outputON) {
        SET_ZERO(PORTD, PORTD6);
        OCR0A=highTime;
    } else {
        SET_ONE(PORTD, PORTD6);
        OCR0A=lowTime;
    }
    outputON = !outputON;

}

ISR(TWI0_vect) {
	if (TWI_State == SEND_SLA) {
		if ((TWSR0 & 0xF8) != 0x08) {
			TWI_Error = 1;
		}
		// UART_putstring("send_SLA");
		TWDR0 = (IR_ADDRESS << 1) | 0b0; // SLA_W
		TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
		TWI_State = SEND_DATA_ADDR;
	} else if (TWI_State == SEND_DATA_ADDR) {
		if ((TWSR0 & 0xF8) != 0x18) {
			TWI_Error = 2;
		}
		TWDR0 = TWI_addr_to_read;
		TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
		TWI_State = SEND_DATA_STOP;
	} else if (TWI_State == SEND_DATA_STOP) {
		if ((TWSR0 & 0xF8) != 0x28) {
			TWI_Error = 3;
		}
		// TWCR0 = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE); // stop
		TWI_State = RECEIVE_DATA_START;
	}else if (TWI_State == RECEIVE_DATA_START) {
		TWI_data_received = 5;
		// if ((TWSR0 & 0xF8) != 0x) {
		// 	TWI_Error = 4;
		// }
		TWCR0 = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE); // (repeated) start
		TWI_State = RECEIVE_DATA_SEND_SLA;
	} else if (TWI_State == RECEIVE_DATA_SEND_SLA) {
		if ((TWSR0 & 0xF8) != 0x10) {
			TWI_Error = 5;
		}
		TWDR0 = (IR_ADDRESS << 1) | 0b1; // SLA_R
		TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
		TWI_State = RECEIVE_DATA_SENT_SLA;
	} else if (TWI_State == RECEIVE_DATA_SENT_SLA) {
		if ((TWSR0 & 0xF8) != 0x40) {
			TWI_Error = 6;
		} 
		TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);// | (1 << TWEA);
		TWI_State = RECEIVE_DATA_RECEIVE;
	} else if (TWI_State == RECEIVE_DATA_RECEIVE) {
		TWI_data_received = TWDR0;
		if ((TWSR0 & 0xF8) != 0x58) {
			TWI_Error = 7;
		}
		TWCR0 = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // stop
		TWI_State = SEND_MESSAGE;
	}
}
