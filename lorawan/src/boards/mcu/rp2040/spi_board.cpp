// #define ARDUINO_ARCH_RP2040
#ifdef ARDUINO_ARCH_RP2040
#include <SPI.h>
#include "boards/mcu/board.h"

// SPIClass SPI_LORA = &SPI;
MbedSPI SPI_LORA(12, 11, 10);

void initSPI(void)
{
	// SPI_LORA.setRX(_hwConfig.PIN_LORA_MISO);
	// SPI_LORA.setCS(_hwConfig.PIN_LORA_NSS);
	// SPI_LORA.setSCK(_hwConfig.PIN_LORA_SCLK);
	// SPI_LORA.setTX(_hwConfig.PIN_LORA_MOSI);
	SPI_LORA.begin();
	pinMode(_hwConfig.PIN_LORA_NSS, OUTPUT);
	digitalWrite(_hwConfig.PIN_LORA_NSS, HIGH);
}

#define LORAWAN_SPI_ACCESS_SPICLASS
#include "boards/mcu/spi_board_spiclass.cpp"

#endif