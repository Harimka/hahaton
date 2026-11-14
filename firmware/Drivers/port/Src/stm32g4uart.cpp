/**
 * @file    stm32g4uart.cpp
 * @author  team-04
 * @brief   STM32G4 HAL UART / LPUART adapter implementation.
 */

#include "stm32g4uart.h"
#include <stdio.h>

namespace team04 {

Stm32G4Uart::Stm32G4Uart(UART_HandleTypeDef& huart)
    : huart_(huart)
{
}

uint16_t Stm32G4Uart::write(const uint8_t* data, uint16_t length)
{
    if (!data || length == 0U) {
        return 0U;
    }

    if (HAL_UART_Transmit(&huart_, const_cast<uint8_t*>(data), length, 100U) == HAL_OK) {
        return length;
    }

    return 0U;
}

void Stm32G4Uart::print(const char* str)
{
    if (!str) {
        return;
    }

    uint16_t len = 0U;
    while (str[len] != '\0') {
        ++len;
    }

    write(reinterpret_cast<const uint8_t*>(str), len);
}

void Stm32G4Uart::printf(const char* fmt, ...)
{
    if (!fmt) {
        return;
    }

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    const int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (written > 0) {
        const uint16_t len = (written < static_cast<int>(sizeof(buffer)))
                             ? static_cast<uint16_t>(written)
                             : static_cast<uint16_t>(sizeof(buffer) - 1U);
        write(reinterpret_cast<const uint8_t*>(buffer), len);
    }
}

} // namespace team04

/***************************** END OF FILE ************************************/
