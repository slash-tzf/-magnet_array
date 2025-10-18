#include "soft_i2c.h"

static void soft_i2c_delay(const soft_i2c_bus_t *bus)
{
	uint32_t cycles = (bus != NULL && bus->delay_cycles != 0u) ? bus->delay_cycles : 32u;
	for (volatile uint32_t i = 0; i < cycles; ++i) {
		__NOP();
	}
}

static void soft_i2c_scl_high(soft_i2c_bus_t *bus)
{
	HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
	soft_i2c_delay(bus);
}

static void soft_i2c_scl_low(soft_i2c_bus_t *bus)
{
	HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
	soft_i2c_delay(bus);
}

static void soft_i2c_sda_high(soft_i2c_bus_t *bus)
{
	HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
	soft_i2c_delay(bus);
}

static void soft_i2c_sda_low(soft_i2c_bus_t *bus)
{
	HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_RESET);
	soft_i2c_delay(bus);
}

static void soft_i2c_start(soft_i2c_bus_t *bus)
{
	soft_i2c_sda_high(bus);
	soft_i2c_scl_high(bus);
	soft_i2c_sda_low(bus);
	soft_i2c_scl_low(bus);
}

static void soft_i2c_stop(soft_i2c_bus_t *bus)
{
	soft_i2c_sda_low(bus);
	soft_i2c_scl_high(bus);
	soft_i2c_sda_high(bus);
}

static bool soft_i2c_write_byte(soft_i2c_bus_t *bus, uint8_t byte)
{
	for (int bit = 7; bit >= 0; --bit) {
		soft_i2c_scl_low(bus);
		if ((byte >> bit) & 0x01u) {
			soft_i2c_sda_high(bus);
		} else {
			soft_i2c_sda_low(bus);
		}
		soft_i2c_scl_high(bus);
	}

	soft_i2c_scl_low(bus);
	soft_i2c_sda_high(bus); // Release SDA for ACK
	soft_i2c_scl_high(bus);
	GPIO_PinState ack = HAL_GPIO_ReadPin(bus->sda_port, bus->sda_pin);
	soft_i2c_scl_low(bus);
	return ack == GPIO_PIN_RESET;
}

static uint8_t soft_i2c_read_byte(soft_i2c_bus_t *bus, bool ack)
{
	uint8_t value = 0;

	soft_i2c_sda_high(bus); // Release SDA
	for (int bit = 7; bit >= 0; --bit) {
		soft_i2c_scl_low(bus);
		soft_i2c_scl_high(bus);
		if (HAL_GPIO_ReadPin(bus->sda_port, bus->sda_pin) == GPIO_PIN_SET) {
			value |= (1u << bit);
		}
	}

	soft_i2c_scl_low(bus);
	if (ack) {
		soft_i2c_sda_low(bus);
	} else {
		soft_i2c_sda_high(bus);
	}
	soft_i2c_scl_high(bus);
	soft_i2c_scl_low(bus);
	soft_i2c_sda_high(bus); // Release SDA after ACK/NACK phase

	return value;
}

void soft_i2c_init(soft_i2c_bus_t *bus)
{
	if (bus == NULL) {
		return;
	}

	GPIO_InitTypeDef gpio = {0};
	gpio.Mode = GPIO_MODE_OUTPUT_OD;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;

	gpio.Pin = bus->scl_pin;
	HAL_GPIO_Init(bus->scl_port, &gpio);

	gpio.Pin = bus->sda_pin;
	HAL_GPIO_Init(bus->sda_port, &gpio);

	HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
	soft_i2c_delay(bus);
}

bool soft_i2c_write(soft_i2c_bus_t *bus, uint16_t address, const uint8_t *data, size_t length)
{
	if (bus == NULL) {
		return false;
	}
	if (length > 0u && data == NULL) {
		return false;
	}

	soft_i2c_start(bus);
	if (!soft_i2c_write_byte(bus, (uint8_t)(address & 0xFEu))) {
		soft_i2c_stop(bus);
		return false;
	}

	for (size_t i = 0; i < length; ++i) {
		if (!soft_i2c_write_byte(bus, data[i])) {
			soft_i2c_stop(bus);
			return false;
		}
	}

	soft_i2c_stop(bus);
	return true;
}

bool soft_i2c_write_then_read(soft_i2c_bus_t *bus, uint16_t address, const uint8_t *tx_data, size_t tx_length, uint8_t *rx_data, size_t rx_length)
{
	if (bus == NULL) {
		return false;
	}
	if ((tx_length == 0u) && (rx_length == 0u)) {
		return true;
	}
	if (tx_length > 0u && tx_data == NULL) {
		return false;
	}
	if (rx_length > 0u && rx_data == NULL) {
		return false;
	}

	if (tx_length > 0u) {
		soft_i2c_start(bus);
		if (!soft_i2c_write_byte(bus, (uint8_t)(address & 0xFEu))) {
			soft_i2c_stop(bus);
			return false;
		}
		for (size_t i = 0; i < tx_length; ++i) {
			if (!soft_i2c_write_byte(bus, tx_data[i])) {
				soft_i2c_stop(bus);
				return false;
			}
		}
	}

	if (rx_length > 0u) {
		soft_i2c_start(bus);
		if (!soft_i2c_write_byte(bus, (uint8_t)(address | 0x01u))) {
			soft_i2c_stop(bus);
			return false;
		}

		for (size_t i = 0; i < rx_length; ++i) {
			bool ack = (i + 1u) < rx_length;
			rx_data[i] = soft_i2c_read_byte(bus, ack);
		}
	}

	soft_i2c_stop(bus);
	return true;
}
