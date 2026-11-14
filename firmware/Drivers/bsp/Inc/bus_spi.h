/*******************************************************************************
 * @file    bus_spi.h
 * @author  team-04 (Hackathon 2026)
 * @brief   Hardware-independent synchronous/DMA SPI bus interface definition.
 ******************************************************************************/

#ifndef BUS_SPI_H_
#define BUS_SPI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Generic SPI device handle interface.
 * Decouples hardware SPI peripheral (HAL/LL/Bitbang) from sensor drivers.
 */
typedef struct spi_bus_device_s {
    void *user_ctx; ///< Pointer to peripheral instance (e.g. SPI_HandleTypeDef*)
    void (*cs_select)(void);
    void (*cs_deselect)(void);
    bool (*transfer_sync)(struct spi_bus_device_s *dev, const uint8_t *tx, uint8_t *rx, uint16_t len);
    bool (*read_reg)(struct spi_bus_device_s *dev, uint8_t reg, uint8_t *data, uint16_t len);
    bool (*write_reg)(struct spi_bus_device_s *dev, uint8_t reg, const uint8_t *data, uint16_t len);
} spi_bus_device_t;

#ifdef __cplusplus
}
#endif

#endif /* BUS_SPI_H_ */
/***************************** END OF FILE ************************************/
