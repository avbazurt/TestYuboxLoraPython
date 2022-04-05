#if defined TARGET_PIGPIO
#include "boards/mcu/board.h"
#include <pigpio.h>

int SPI_HANDLE = PI_NO_HANDLE;

void initSPI(void)
{
    SPI_HANDLE = spiOpen(
        0,      // CE0, pin 8
        2000000,
        0);
    if (SPI_HANDLE < 0) {
        fprintf(stderr, "spiOpen fail: %d\n", SPI_HANDLE);
    }
}

#define MAX_SPI_BUFFER 512

void transferSPI(
    const uint8_t * tx_header, uint32_t tx_header_size,
    const uint8_t * tx_body, uint32_t tx_body_size,
    uint8_t * rx_body, uint32_t rx_body_size
)
{
    // The pigpio library does not support a SPI transfer API that will not
    // touch the SS pin so that multiple discontinuous buffers can be written
    // and then read from the same SPI slave in a single transaction. Therefore,
    // a single SPI buffer must be used for the entirety of the SPI transfer.
    static char spi_buffer[MAX_SPI_BUFFER];

    if (SPI_HANDLE < 0) {
        fprintf(stderr, "transferSPI no handle opened!\n");
        return;
    }

    unsigned transfer_size = 0;
    unsigned rx_pos = 0;

    // Check if entirety of transfer fits in static buffer, just to be sure
    if (tx_header != NULL && tx_header_size > 0) transfer_size += tx_header_size;
    if (tx_body != NULL && tx_body_size > 0) transfer_size += tx_body_size;
    if (rx_body != NULL && rx_body_size > 0) transfer_size += rx_body_size;
    if (transfer_size > MAX_SPI_BUFFER) {
        fprintf(stderr, "transferSPI no space for this transfer! available=%u required=%u\n", MAX_SPI_BUFFER, transfer_size);
        return;
    }

    // TODO: take lock to prevent concurrent access from different threads!

    // Copy header and body to be transferred
    transfer_size = 0;
    if (tx_header != NULL && tx_header_size > 0) {
        memcpy(spi_buffer + transfer_size, tx_header, tx_header_size);
        transfer_size += tx_header_size;
    }
    if (tx_body != NULL && tx_body_size > 0) {
        memcpy(spi_buffer + transfer_size, tx_body, tx_body_size);
        transfer_size += tx_body_size;
    }
    if (rx_body != NULL && rx_body_size > 0) {
        // Ensure that only zeros are transmitted when receiving data
        memset(spi_buffer + transfer_size, 0, rx_body_size);
        rx_pos = transfer_size;
        transfer_size += rx_body_size;
    }

    // Run SPI transfer, check for errors
    int r = spiXfer(SPI_HANDLE, spi_buffer, spi_buffer, transfer_size);
    if (r < 0) {
        fprintf(stderr, "transferSPI spiXfer fail: %d\n", r);
    }

    // If rx buffer provided, copy transferred bytes
    if (rx_body != NULL && rx_body_size > 0) {
        memcpy(rx_body, spi_buffer + rx_pos, rx_body_size);
    }

    // TODO: free lock
}

void shutdownSPI(void)
{
    if (SPI_HANDLE >= 0) {
        spiClose(SPI_HANDLE);
    }
    SPI_HANDLE = PI_NO_HANDLE;
}


#endif
