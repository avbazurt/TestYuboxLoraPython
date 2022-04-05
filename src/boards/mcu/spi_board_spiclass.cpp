/**
 * This file gathers all the SPI access implementation methods in one place,
 * so that they are not repeated across all MCU implementation files that
 * use Arduino SPIClass-compatible objects for SPI access. This file should
 * be included from spi_board.cpp for each architecture that uses an Arduino-
 * compatible SPIClass.
 */

#ifdef LORAWAN_SPI_ACCESS_SPICLASS

// The entirety of the following code assumes SPI_LORA has been declared

SPISettings spiSettings = SPISettings(2000000, MSBFIRST, SPI_MODE0);

void transferSPI(
	const uint8_t * tx_header, uint32_t tx_header_size,
	const uint8_t * tx_body, uint32_t tx_body_size,
	uint8_t * rx_body, uint32_t rx_body_size)
{
	digitalWrite(_hwConfig.PIN_LORA_NSS, LOW);

	SPI_LORA.beginTransaction(spiSettings);

	// Transmit (possibly fixed-size) header
	if (tx_header != NULL && tx_header_size > 0) {
		for (uint16_t i = 0; i < tx_header_size; i++) {
			SPI_LORA.transfer(tx_header[i]);
		}
	}

	// Transmit body, if there is one
	if (tx_body != NULL && tx_body_size > 0) {
		for (uint16_t i = 0; i < tx_body_size; i++) {
			SPI_LORA.transfer(tx_body[i]);
		}
	}

	// Receive body, if expected
	if (rx_body != NULL && rx_body_size > 0) {
		for (uint16_t i = 0; i < rx_body_size; i++) {
			rx_body[i] = SPI_LORA.transfer(0x00);
		}
	}

	SPI_LORA.endTransaction();
	digitalWrite(_hwConfig.PIN_LORA_NSS, HIGH);
}

void shutdownSPI(void)
{
	pinMode(_hwConfig.PIN_LORA_NSS, INPUT);
}

#endif // LORAWAN_SPI_ACCESS_SPICLASS
