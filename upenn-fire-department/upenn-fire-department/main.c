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

uint8_t servoHighTime = 170;
uint8_t servoLowTime = 115;

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
#define GOOD_DRIVE_DIS ((MIN_DRIVE_DIS_US < US_avg))
// && (US_avg < MAX_DRIVE_DIS_US)
uint8_t TWI_watchdog = 0;

bool timer3_increasing_ocr = true;
uint16_t timer3_ocr_val = TIMER3_OCR_MIN;
uint16_t timer3_ocr_counter_counter = 0;

bool leds_on = false;
uint16_t leds_on_counter = 0;

uint8_t outputCounter = 0;

uint32_t timeDifferenceUS(uint32_t startTime, uint32_t endTime, uint8_t overflowCount) {
	return ((endTime + (((uint32_t)overflowCount) * ((uint32_t)UINT16_MAX))) - startTime) / 2;
}

uint16_t counter = 0;
uint8_t outputCounter2 = 0;
uint8_t outputCounter3 = 0;

bool autonomous = true;

#define MOVE_FL() SET_ONE(PORTB, PORTB4); \
					SET_ZERO(PORTB, PORTB5)

#define MOVE_BL() SET_ZERO(PORTB, PORTB4); \
					SET_ONE(PORTB, PORTB5)

#define MOVE_NL() SET_ZERO(PORTB, PORTB4); \
					SET_ZERO(PORTB, PORTB5)

#define MOVE_FR() SET_ONE(PORTD, PORTD3); \
					SET_ZERO(PORTD, PORTD4)

#define MOVE_BR() SET_ZERO(PORTD, PORTD3); \
					SET_ONE(PORTD, PORTD4)

#define MOVE_NR() SET_ZERO(PORTD, PORTD3); \
					SET_ZERO(PORTD, PORTD4)

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

	// output for timer3 b (buzzer)
	SET_ONE(DDRD, DDRD2);
	SET_ONE(PORTD, PORTD2);
	// output for LEDs
	SET_ONE(DDRE, DDRE0);

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
	SET_ZERO(DDRB, DDRB1); // Autonomous
	SET_ZERO(DDRB, DDRB2); // Manual trigger

	// fire detected led output
	SET_ONE(DDRD, DDRD7);

	/** FEATHER INTERRUPTS **/

	// Enable pin change interrupt
	SET_ONE(PCICR, PCIE1);

	// Enable it for appropriate (feather input) pins
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

	/** TIMER 3 **/

	// Set a prescaler of 64 (62.5kHz)
	SET_ZERO(TCCR3B, CS32);
	SET_ONE(TCCR3B, CS31);
	SET_ONE(TCCR3B, CS30);

	// Set CTC mode (compare with OCR3A)
	SET_ZERO(TCCR3B, WGM33);
	SET_ONE(TCCR3B, WGM32);
	SET_ZERO(TCCR3A, WGM31);
	SET_ZERO(TCCR3A, WGM30);

	// Toggle OC3B (PORTD2) on compare match
	SET_ZERO(TCCR3A, COM3B1);
	SET_ONE(TCCR3A, COM3B0);

	// enable overflow interrupt
	// SET_ONE(TIMSK3, TOIE3);

	// enable compare match interrupt
	SET_ONE(TIMSK3, OCIE3A);

	// 650Hz
	OCR3A = TIMER3_OCR_MIN;
	timer3_increasing_ocr = true;
	timer3_ocr_val = TIMER3_OCR_MIN;

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
		// UART_send('m');
		if (TWI_state == SEND_MESSAGE) {
			// UART_send('y');
			if (!TWI_first_time) {
				if (temperature_idx == 63 && TWI_read_high_byte) {
					temperature_max = 0;
					for (uint8_t i = 0; i < 64; i++) {
						if (i == 48 || i == 56 || i == 8 || i == 15) {
							continue;
						}
						if (temperature_values[i] > temperature_max) {
							temperature_max = temperature_values[i];
							temperature_max_idx = i;
						}
						// snprintf(uart_buf, sizeof(uart_buf), "%02hu: %u\n", i, temperature_values[i]);
						// UART_putstring(uart_buf);
					}
					// snprintf(uart_buf, sizeof(uart_buf), "max temp: %u\n", temperature_max);
					// UART_putstring(uart_buf);
					// outputCounter3 = (outputCounter3 + 1) % 40;
					// if (outputCounter3) {
					// 	snprintf(uart_buf, sizeof(uart_buf), "max temp:%03u\t idx: %h02u\t col:%hu\t row:%hu\n", temperature_max, temperature_max_idx, temperature_max_col, temperature_max_row);
					// 	UART_putstring(uart_buf);
					// }
					fire_detected = temperature_max > FIRE_THRESHOLD;
					if (fire_detected) {
						UART_send('F');
						SET_ONE(PORTD, PORTD7);
					} else {
						SET_ZERO(PORTD, PORTD7);
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
			TWI_watchdog = 0;
			TWCR0 = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE); // send start
		}
		if (TWI_watchdog > 9) {
			TWI_first_time = true;
			TWI_state = SEND_MESSAGE;
		}
		autonomous = GET_BIT(PINB, PINB1);//;
		handleMoveLogic();
		counter = (counter + 1) % 65000;
		if (counter == 0) {
			if (fire_detected) {
				if (outputCounter2 == 0) {
					// UART_putstring("Fire Detected!\n");
				}
			}
			if (GOOD_FIRE_DIS) {
				if (outputCounter2 == 0) {
					// UART_putstring("Good fire distance!\n");
				}
			}
			if ((2 <= temperature_max_col) && (temperature_max_col <= 5)) {
				if (outputCounter2 == 0) {
					// UART_putstring("Aimed!\n");
				}
			}
			if ((autonomous && fire_detected && GOOD_FIRE_DIS && (1 <= temperature_max_col) && (temperature_max_col <= 6))
			|| (!autonomous && fire_detected && US_avg > MIN_FIRE_DIS_US)) {
				if (outputCounter2 == 0) {
					// UART_putstring("Firing!\n");
				}
				if (triggerPulled) {
					servoHighTime = 170;
					servoLowTime = 115;
				} else {
					servoHighTime = 140;
					servoLowTime = 100;
				}
				triggerPulled = !triggerPulled;
			}
			outputCounter2 = (outputCounter2 + 1) % 10;
		}
    }
}

void handleMoveLogic(void) {
	// PORTB4  FL
	// PORTB5  BL
	// PORTD3  FR
	// PORTD4  BR
	// UART_send('h');
	
	// don't move
	// if (fire_detected) {
	// 	MOVE_NL();
	// 	MOVE_NR();
	// } else {
	// 	// BL
	// 	MOVE_BL();
	// 	// FR
	// 	MOVE_FR();
	// }
	
	if (autonomous) {
		if (fire_detected) {
			if (false ) { // temperature_max_col <= 0
				// turn right
				// FL
				MOVE_FL();
				// BR
				MOVE_BR();
			} else if (false) { // temperature_max_col >= 7
				// turn left
				// BL
				MOVE_BL();
				// FR
				MOVE_FR();
			} else {
				if (false) { // US_avg > MAX_FIRE_DIS_US && US_avg > MIN_DRIVE_DIS_US
					// forward
					MOVE_FL();
					MOVE_FR();
				} else {
					// don't move
					MOVE_NL();
					MOVE_NR();
				}
			}
		} else {
			if (GOOD_DRIVE_DIS) {
				// forward
				// FL
				MOVE_FL();
				// FR
				MOVE_FR();
			} else {
				// turn right
				// FL
				MOVE_FL();
				// BR
				MOVE_BR();
			}
		}
	} else {
		if (GET_BIT(PINC, PINC0) && GOOD_DRIVE_DIS) { // FL //  
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
		if (GET_BIT(PINC, PINC2) && GOOD_DRIVE_DIS) { // FR //  
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
	// UART_send('t');
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
	// UART_send('e');
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
		
		// uint16_t cm = US_avg / 58;
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
	// UART_send('o');
	echoOverflows++;
	if (trigSent && echoOverflows > 5 && (!echoStarted)) {
		// UART_send('X');
		trigSent = false; // resend trig
		echoOverflows = 0;
		// uint32_t diffUS = 6000;
		// US_avg_values[US_avg_idx] = diffUS;
		// US_avg_idx = (US_avg_idx + 1) % US_AVG_NUM_VALS;
		// US_avg = 0;
		// for (int i = 0; i < US_AVG_NUM_VALS; i++) {
	 	// 	US_avg += US_avg_values[i];
		// }
		// US_avg = US_avg / US_AVG_NUM_VALS;
	}
}

// servo control
ISR(TIMER0_OVF_vect) {
	// UART_send('s');
    if (servoOutputOn) {
        SET_ZERO(PORTD, PORTD6);
        OCR0A=servoHighTime;
    } else {
        SET_ONE(PORTD, PORTD6);
        OCR0A=servoLowTime;
    }
    servoOutputOn = !servoOutputOn;
}

// do buzzer and LEDs
ISR(TIMER3_COMPA_vect) {
	// UART_send('{');
	// UART_send('S');
	timer3_ocr_counter_counter = (timer3_ocr_counter_counter + 1) % TIMER3_OCR_MOV_RATE;
	if (timer3_ocr_counter_counter == 0) {
		if (timer3_increasing_ocr) {
			timer3_ocr_val++;
		} else {
			timer3_ocr_val--;
		}
		if (timer3_ocr_val >= TIMER3_OCR_MAX) {
			timer3_increasing_ocr = false;
		}
		if (timer3_ocr_val <= TIMER3_OCR_MIN) {
			timer3_increasing_ocr = true;
		}
		OCR3A = timer3_ocr_val;
		leds_on_counter = (leds_on_counter + 1) % LED_FLICKER_PERIOD;
		if (leds_on_counter == 0) {
			leds_on = !leds_on;
			// UART_send('l');
			if (leds_on) {
				SET_ONE(PORTE, PORTE0);
			} else {
				SET_ZERO(PORTE, PORTE0);
			}
			TWI_watchdog++;
		}
	}
}

// TWI control / IR camera
ISR(TWI0_vect) {
	TWI_watchdog = 0;
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
