#ifndef _SPI_BOARD_H
#define _SPI_BOARD_H
#if defined ESP8266 || defined ESP32
#include "boards/mcu/espressif/spi_board.h"
#elif defined(NRF52_SERIES)
#include "boards/mcu/nrf52832/spi_board.h"
#elif defined(TARGET_PIGPIO)
#include "boards/mcu/pigpio/spi_board.h"
#else
#include "boards/mcu/rp2040/spi_board.h"
#endif

void initSPI(void);

void transferSPI(
    const uint8_t * tx_header, uint32_t tx_header_size,
    const uint8_t * tx_body, uint32_t tx_body_size,
    uint8_t * rx_body, uint32_t rx_body_size
);

void shutdownSPI(void);

#endif