#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SET_ONE(REG, FLAG) REG |= (1 << FLAG)
#define SET_ZERO(REG, FLAG) REG &= ~(1 << FLAG)
#define SET_FLIP(REG, FLAG) REG ^= (1 << FLAG)

#define GET_BIT(REG, FLAG) (REG & (1 << FLAG))

#define IR_ADDRESS 0x69
#define IR_TEMP_REG 0x80
#define US_AVG_NUM_VALS 4
#define FIRE_THRESHOLD 110

typedef enum TWI_STATE_t {
	SEND_MESSAGE,
	SEND_SLA,
	SEND_DATA_ADDR,
	SEND_DATA_STOP,
	RECEIVE_DATA_START,
	RECEIVE_DATA_SEND_SLA,
	RECEIVE_DATA_SENT_SLA,
	RECEIVE_DATA_RECEIVE
} twi_state;

uint32_t timeDifferenceUS(uint32_t startTime, uint32_t endTime, uint8_t overflowCount);