/**
 * @file    stm32g4uart.h
 * @author  team-04
 * @brief   STM32G4 HAL UART / LPUART adapter for debug logging.
 */

#ifndef TEAM04_STM32G4UART_H_
#define TEAM04_STM32G4UART_H_

#include <stdarg.h>
#include "stm32g4xx_hal.h"
#include "serialdrv.h"

namespace team04 {

/**
 * @brief Concrete STM32G4 UART logger adapter.
 */
class Stm32G4Uart final : public SerialDrv {
public:
    explicit Stm32G4Uart(UART_HandleTypeDef& huart);
    ~Stm32G4Uart() override = default;

    Stm32G4Uart(const Stm32G4Uart&) = delete;
    Stm32G4Uart& operator=(const Stm32G4Uart&) = delete;

    uint16_t write(const uint8_t* data, uint16_t length) override;
    void print(const char* str) override;

    /**
     * @brief Safe bounded printf to UART without dynamic memory allocation.
     * @param fmt Printf format string.
     */
    void printf(const char* fmt, ...);

private:
    UART_HandleTypeDef& huart_;
};

} // namespace team04

#endif // TEAM04_STM32G4UART_H_

/***************************** END OF FILE ************************************/
