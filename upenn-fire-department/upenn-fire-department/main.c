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

char uart_buf[80];

bool trigSent = false;
bool trigOutputHigh = false;
bool echoStarted = false;
bool echoEnded = false;

uint8_t echoOverflows = 0;
uint16_t echoStartTime = 0;
uint16_t echoEndTime = 0;

bool servoOutputOn = false;

uint8_t servoHighTime = 50;
uint8_t servoLowTime = 50;

bool triggerPulled = true;

uint8_t TWI_addr_to_read = 0;

twi_state TWI_state = SEND_MESSAGE;

uint8_t TWI_Error = 0;

bool TWI_first_time = true;

uint8_t TWI_read_high_byte = false;
uint8_t TWI_data_received;

uint16_t temperature_values[64];
uint8_t temperature_idx = 0;
uint16_t temperature_max = 0;
uint8_t temperature_max_idx = 0;
#define temperature_max_row (temperature_max_idx / 8)
#define temperature_max_col (temperature_max_idx % 8)
bool fire_detected = false;

uint32_t US_avg_values[US_AVG_NUM_VALS];
uint32_t US_avg = 0;
uint8_t US_avg_idx = 0;
#define GOOD_FIRE_DIS ((MIN_FIRE_DIS_US < US_avg) && (US_avg < MAX_FIRE_DIS_US))
#define GOOD_DRIVE_DIS ((MIN_DRIVE_DIS_US < US_avg) && (US_avg < MAX_DRIVE_DIS_US))

uint8_t outputCounter = 0;

uint32_t timeDifferenceUS(uint32_t startTime, uint32_t endTime, uint8_t overflowCount) {
	return ((endTime + (((uint32_t)overflowCount) * ((uint32_t)UINT16_MAX))) - startTime) / 2;
}

uint16_t counter = 0;
uint8_t outputCounter2 = 0;

bool autonomous = false;

void initialize(void) {
	cli();

	for (uint8_t i = 0; i < 64; i++) {
		temperature_values[i] = 0;
	}
	
	UART_init(BAUD_PRESCALER);

	// input for timer1 (input capture) (echo measuring)
	SET_ZERO(DDRB, DDRB0);
	// output for timer2 (trig send) (10us)
	SET_ONE(DDRB, DDRB3);

    // output for timer0 PWM for servo
    SET_ONE(DDRD, DDRD6);

	// set outputs for TWI
	SET_ONE(DDRC, DDRC4); // SDA0
	SET_ONE(DDRC, DDRC5); // SCL0

	// Motor drive outputs
	SET_ONE(DDRB, DDRB4); // FL
	SET_ONE(DDRB, DDRB5); // BL
	SET_ONE(DDRD, DDRD3); // FR
	SET_ONE(DDRD, DDRD4); // BR

	// Feather inputs
	// PCINT[8..11]
	SET_ZERO(DDRC, DDRC0); // FL
	SET_ZERO(DDRC, DDRC1); // BL
	SET_ZERO(DDRC, DDRC2); // FR
	SET_ZERO(DDRC, DDRC3); // BR

	/** FEATHER INTERRUPTS **/
	SET_ONE(PCICR, PCIE1);

	SET_ONE(PCMSK1, PCINT8);
	SET_ONE(PCMSK1, PCINT9);
	SET_ONE(PCMSK1, PCINT10);
	SET_ONE(PCMSK1, PCINT11);
	
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
		if (TWI_state == SEND_MESSAGE) {
			// UART_sensd('y');
			if (!TWI_first_time) {
				if (temperature_idx == 63 && TWI_read_high_byte) {
					temperature_max = 0;
					for (uint8_t i = 0; i < 64; i++) {
						if (temperature_values[i] > temperature_max) {
							temperature_max = temperature_values[i];
							temperature_max_idx = i;
						}
						// snprintf(uart_buf, sizeof(uart_buf), "%02hu: %u\n", i, temperature_values[i]);
						// UART_putstring(uart_buf);
					}
					// snprintf(uart_buf, sizeof(uart_buf), "max temp: %u\n", temperature_max);
					// UART_putstring(uart_buf);
					// snprintf(uart_buf, sizeof(uart_buf), "max temp idx: %hu\n", temperature_max_idx);
					// UART_putstring(uart_buf);
					fire_detected = temperature_max > FIRE_THRESHOLD;
					if (fire_detected) {
						UART_send('F');
					}
					// while (!(TWCR0 & (1 << TWINT)));
				}
				if (TWI_read_high_byte) {
					temperature_values[temperature_idx] = (temperature_values[temperature_idx] & 0x00FF) | ((TWI_data_received & 0x0F) << 8);
					temperature_idx = (temperature_idx + 1) % 64;
				} else {
					temperature_values[temperature_idx] = (temperature_values[temperature_idx] & 0xFF00) | TWI_data_received;
				}
				TWI_read_high_byte = !TWI_read_high_byte;
			}
			TWI_first_time = false;
			TWI_state = SEND_SLA;
			TWI_addr_to_read = IR_TEMP_REG + (temperature_idx << 1) + (TWI_read_high_byte ? 1 : 0);
			// snprintf(uart_buf, sizeof(uart_buf), "addr_to_read: %hX\n", TWI_addr_to_read);
			// UART_putstring(uart_buf);
			TWCR0 = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE); // send start
		}
		handleMoveLogic();
		counter = (counter + 1) % 20000;
		if (counter == 0) {
			if (fire_detected) {
				if (outputCounter2 == 0) {
					UART_putstring("Fire Detected!\n");
				}
			}
			if (GOOD_FIRE_DIS) {
				if (outputCounter2 == 0) {
					// UART_putstring("Ready to fire!\n");
				}
			}
			if (fire_detected && GOOD_FIRE_DIS) {
				if (outputCounter2 == 0) {
					// UART_putstring("Firing!\n");
				}
				if (triggerPulled) {
					servoHighTime = 70;
					servoLowTime = 35;
				} else {
					servoHighTime = 50;
					servoLowTime = 50;
				}
				triggerPulled = !triggerPulled;
			}
			outputCounter2 = (outputCounter2 + 1) % 50;
		}
    }
}

void handleMoveLogic(void) {
	// PORTB4  FL
	// PORTB5  BL
	// PORTD3  FR
	// PORTD4  BR
	if (autonomous) {
		if (fire_detected) {
			if (temperature_max_col <= 1) {
				// turn right
				// FL
				// BR
			} else if (temperature_max_col >= 6) {
				// turn left
				// BL
				// FR
			} else {
				if (US_avg < MAX_FIRE_DIS_US) {
					// forward
				} else {
					// don't move
				}
			}
		} else {
			if (GOOD_DRIVE_DIS) {
				// forward
				// FL
				SET_ONE(PORTB, PORTB4);
				SET_ZERO(PORTB, PORTB5);
				// FR
				SET_ONE(PORTD, PORTD3);
				SET_ZERO(PORTD, PORTD4);
			} else {
				// turn right
			}
		}
	} else {
		if (GET_BIT(PINC, PINC0) && GOOD_DRIVE_DIS) { // FL
			// UART_putstring("FL\t");
			SET_ONE(PORTB, PORTB4);
			SET_ZERO(PORTB, PORTB5);
		} else if (GET_BIT(PINC, PINC1)) { // BL
			// UART_putstring("BL\t");
			SET_ZERO(PORTB, PORTB4);
			SET_ONE(PORTB, PORTB5);
		} else { // No left
			// UART_putstring("NL\t");
			SET_ZERO(PORTB, PORTB4);
			SET_ZERO(PORTB, PORTB5);
		}
		if (GET_BIT(PINC, PINC2) && GOOD_DRIVE_DIS) { // FR
			// UART_putstring("FR\n");
			SET_ONE(PORTD, PORTD3);
			SET_ZERO(PORTD, PORTD4);
		} else if (GET_BIT(PINC, PINC3)) { // BR
			// UART_putstring("BR\n");
			SET_ZERO(PORTD, PORTD3);
			SET_ONE(PORTD, PORTD4);
		} else { // No right
			// UART_putstring("NR\n");
			SET_ZERO(PORTD, PORTD3);
			SET_ZERO(PORTD, PORTD4);
		}
	}
}

// motor control when feather pins change
ISR(PCINT1_vect) {
}


// send trigger
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

// capture echo
ISR(TIMER1_CAPT_vect) {
	// flip checking for rising/falling edge
	// SET_FLIP(TCCR1B, ICES1);
	if (GET_BIT(PINB, PINB0)) {
		echoStartTime = ICR1;
		echoStarted = true;
		echoOverflows = 0;
		// interrupt on falling edge
		SET_ZERO(TCCR1B, ICES1);
	} else {
		echoEndTime = ICR1;
		echoEnded = true;
		trigSent = false;
		// interrupt on rising edge
		SET_ONE(TCCR1B, ICES1);
		uint32_t diffUS = timeDifferenceUS(echoStartTime, echoEndTime, echoOverflows);
		US_avg_values[US_avg_idx] = diffUS;
		US_avg_idx = (US_avg_idx + 1) % US_AVG_NUM_VALS;
		US_avg = 0;
		for (int i = 0; i < US_AVG_NUM_VALS; i++) {
	 		US_avg += US_avg_values[i];
		}
		US_avg = US_avg / US_AVG_NUM_VALS;
		outputCounter = (outputCounter + 1) % 100;
		
		uint16_t cm = US_avg / 58;
		if (outputCounter == 0) {
			//snprintf(message, 100, "pressTime=%5u \tendTime=%5u \toverflows=%2u \tdiffUS=%5lu \tcm=%3lu \tin=%3lu\n", echoStartTime, echoEndTime, echoOverflows, diffUS, diffUS / 58, diffUS / 148);
			// snprintf(uart_buf, sizeof(uart_buf), "US_avg=%5lu \tcm=%3u\n", US_avg, cm);
			// UART_putstring(uart_buf);
			// if (cm > 10) {
			// 	if (triggerPulled) {
			// 		servoHighTime = 70;
			// 		servoLowTime = 35;
			// 	} else {
			// 		servoHighTime = 50;
			// 		servoLowTime = 50;
			// 	}
			// 	triggerPulled = !triggerPulled;
			// }
		}
		
	}
}

// too long without echo response
ISR(TIMER1_OVF_vect) {
	echoOverflows++;
	if (trigSent && echoOverflows > 5 && (!echoStarted)) {
		// UART_send('X');
		trigSent = false; // resend trig
		echoOverflows = 0;
	}
}

// servo control
ISR(TIMER0_OVF_vect) {
    if (servoOutputOn) {
        SET_ZERO(PORTD, PORTD6);
        OCR0A=servoHighTime;
    } else {
        SET_ONE(PORTD, PORTD6);
        OCR0A=servoLowTime;
    }
    servoOutputOn = !servoOutputOn;
}

// TWI control / IR camera
ISR(TWI0_vect) {
	// UART_send('x');
	// snprintf(uart_buf, sizeof(uart_buf), "error: %hu\n", TWI_state);
	// UART_putstring(uart_buf);
	if (TWI_state == SEND_SLA) {
		if ((TWSR0 & 0xF8) != 0x08) {
			TWI_Error = 1;
			// snprintf(uart_buf, sizeof(uart_buf), "error1: %hu\n", TWSR0 & 0xF8);
			// UART_putstring(uart_buf);
		}
		// UART_putstring("send_SLA");
		TWDR0 = (IR_ADDRESS << 1) | 0b0; // SLA_W
		TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
		TWI_state = SEND_DATA_ADDR;
	} else if (TWI_state == SEND_DATA_ADDR) {
		if ((TWSR0 & 0xF8) != 0x18) {
			TWI_Error = 2;
		}
		TWDR0 = TWI_addr_to_read;
		TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
		TWI_state = SEND_DATA_STOP;
	} else if (TWI_state == SEND_DATA_STOP) {
		if ((TWSR0 & 0xF8) != 0x28) {
			TWI_Error = 3;
		}
		// TWCR0 = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE); // stop
		TWI_state = RECEIVE_DATA_START;
	}else if (TWI_state == RECEIVE_DATA_START) {
		TWI_data_received = 5;
		// if ((TWSR0 & 0xF8) != 0x) {
		// 	TWI_Error = 4;
		// }
		TWCR0 = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE); // (repeated) start
		TWI_state = RECEIVE_DATA_SEND_SLA;
	} else if (TWI_state == RECEIVE_DATA_SEND_SLA) {
		if ((TWSR0 & 0xF8) != 0x10) {
			TWI_Error = 5;
		}
		TWDR0 = (IR_ADDRESS << 1) | 0b1; // SLA_R
		TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
		TWI_state = RECEIVE_DATA_SENT_SLA;
	} else if (TWI_state == RECEIVE_DATA_SENT_SLA) {
		if ((TWSR0 & 0xF8) != 0x40) {
			TWI_Error = 6;
		} 
		TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);// | (1 << TWEA);
		TWI_state = RECEIVE_DATA_RECEIVE;
	} else if (TWI_state == RECEIVE_DATA_RECEIVE) {
		TWI_data_received = TWDR0;
		if ((TWSR0 & 0xF8) != 0x58) {
			TWI_Error = 7;
		}
		TWCR0 = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN); // stop
		TWI_state = SEND_MESSAGE;
	}
	// // UART_send('x');
	// snprintf(uart_buf, sizeof(uart_buf), "error: %hu\n", TWI_state);
	// UART_putstring(uart_buf);
}
