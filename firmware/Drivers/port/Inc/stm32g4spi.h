/**
 * @file    stm32g4spi.h
 * @author  team-04
 * @brief   STM32G4 HAL SPI adapter implementing SpiDrv interface.
 */

#ifndef TEAM04_STM32G4SPI_H_
#define TEAM04_STM32G4SPI_H_

#include "stm32g4xx_hal.h"
#include "spidrv.h"

namespace team04 {

/**
 * @brief Concrete STM32G4 hardware SPI master adapter.
 */
class Stm32G4Spi final : public SpiDrv {
public:
    Stm32G4Spi(SPI_HandleTypeDef& spi, GPIO_TypeDef& csPort, uint16_t csPin);
    ~Stm32G4Spi() override = default;

    Stm32G4Spi(const Stm32G4Spi&) = delete;
    Stm32G4Spi& operator=(const Stm32G4Spi&) = delete;

    IoStatus setClock(uint32_t maxHz) override;
    void select() override;
    void deselect() override;
    IoStatus transfer(const uint8_t* tx, uint8_t* rx, uint16_t len, uint32_t timeoutMs) override;

private:
    SPI_HandleTypeDef& spi_;
    GPIO_TypeDef& csPort_;
    uint16_t csPin_;
};

} // namespace team04

#endif // TEAM04_STM32G4SPI_H_

/***************************** END OF FILE ************************************/
