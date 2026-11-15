/**
 * @file    stm32g4spi.cpp
 * @author  team-04
 * @brief   STM32G4 HAL SPI adapter implementation.
 */

#include "stm32g4spi.h"

namespace team04 {

namespace {

IoStatus convertStatus(HAL_StatusTypeDef status)
{
    switch (status) {
        case HAL_OK:
            return IoStatus::kOk;
        case HAL_BUSY:
            return IoStatus::kBusy;
        case HAL_TIMEOUT:
            return IoStatus::kTimeout;
        default:
            return IoStatus::kError;
    }
}

} // namespace

Stm32G4Spi::Stm32G4Spi(SPI_HandleTypeDef& spi, GPIO_TypeDef& csPort, uint16_t csPin)
    : spi_(spi)
    , csPort_(csPort)
    , csPin_(csPin)
{
}

void Stm32G4Spi::select()
{
    HAL_GPIO_WritePin(&csPort_, csPin_, GPIO_PIN_RESET);
}

void Stm32G4Spi::deselect()
{
    HAL_GPIO_WritePin(&csPort_, csPin_, GPIO_PIN_SET);
}

IoStatus Stm32G4Spi::setClock(uint32_t maxHz)
{
    if (maxHz == 0U || HAL_SPI_GetState(&spi_) != HAL_SPI_STATE_READY) {
        return IoStatus::kBusy;
    }

    const uint32_t prescalers[] = {
        SPI_BAUDRATEPRESCALER_2,
        SPI_BAUDRATEPRESCALER_4,
        SPI_BAUDRATEPRESCALER_8,
        SPI_BAUDRATEPRESCALER_16,
        SPI_BAUDRATEPRESCALER_32,
        SPI_BAUDRATEPRESCALER_64,
        SPI_BAUDRATEPRESCALER_128,
        SPI_BAUDRATEPRESCALER_256
    };

    const uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t div = 2U;
    uint8_t idx = 0U;

    while (idx < 8U && (static_cast<uint64_t>(maxHz) * div < pclk)) {
        ++idx;
        div *= 2U;
    }

    if (idx >= 8U) {
        return IoStatus::kUnsupported;
    }

    __HAL_SPI_DISABLE(&spi_);
    MODIFY_REG(spi_.Instance->CR1, SPI_CR1_BR, prescalers[idx]);
    spi_.Init.BaudRatePrescaler = prescalers[idx];
    __HAL_SPI_ENABLE(&spi_);

    return IoStatus::kOk;
}

IoStatus Stm32G4Spi::transfer(const uint8_t* tx, uint8_t* rx, uint16_t len, uint32_t timeoutMs)
{
    if (len == 0U || timeoutMs == 0U) {
        return IoStatus::kInvalid;
    }

    if (tx && rx) {
        return convertStatus(HAL_SPI_TransmitReceive(&spi_,
                                                     const_cast<uint8_t*>(tx),
                                                     rx,
                                                     len,
                                                     timeoutMs));
    }

    if (tx && !rx) {
        return convertStatus(HAL_SPI_Transmit(&spi_,
                                              const_cast<uint8_t*>(tx),
                                              len,
                                              timeoutMs));
    }

    // Pure RX: send dummy 0xFF bytes
    uint8_t dummy[32];
    for (uint8_t& b : dummy) {
        b = 0xFFU;
    }

    const uint32_t startTick = HAL_GetTick();
    while (len > 0U) {
        const uint32_t elapsed = HAL_GetTick() - startTick;
        if (elapsed >= timeoutMs) {
            return IoStatus::kTimeout;
        }

        const uint16_t chunk = (len > sizeof(dummy)) ? static_cast<uint16_t>(sizeof(dummy)) : len;
        const HAL_StatusTypeDef halStatus = HAL_SPI_TransmitReceive(&spi_,
                                                                    dummy,
                                                                    rx,
                                                                    chunk,
                                                                    timeoutMs - elapsed);
        if (halStatus != HAL_OK) {
            return convertStatus(halStatus);
        }

        rx += chunk;
        len = static_cast<uint16_t>(len - chunk);
    }

    return IoStatus::kOk;
}

IoStatus Stm32G4Spi::startDma(const uint8_t* tx, uint8_t* rx, uint16_t len)
{
    (void)tx;
    (void)rx;
    (void)len;
    return IoStatus::kUnsupported;
}

IoStatus Stm32G4Spi::abortDma()
{
    return IoStatus::kUnsupported;
}

} // namespace team04

/***************************** END OF FILE ************************************/
