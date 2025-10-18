#pragma once

#include "stm32g0xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	GPIO_TypeDef *scl_port;
	uint16_t scl_pin;
	GPIO_TypeDef *sda_port;
	uint16_t sda_pin;
	uint32_t delay_cycles;
} soft_i2c_bus_t;

void soft_i2c_init(soft_i2c_bus_t *bus);
bool soft_i2c_write(soft_i2c_bus_t *bus, uint16_t address, const uint8_t *data, size_t length);
bool soft_i2c_write_then_read(soft_i2c_bus_t *bus, uint16_t address, const uint8_t *tx_data, size_t tx_length, uint8_t *rx_data, size_t rx_length);
