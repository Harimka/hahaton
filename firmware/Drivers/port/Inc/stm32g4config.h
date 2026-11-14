/** @file stm32g4config.h
 * @author team-04
 * @brief Optional NUCLEO-G474RE resources for a standalone board project.
 */
#ifndef TEAM04_STM32G4CONFIG_H_
#define TEAM04_STM32G4CONFIG_H_
#include "stm32g4xx_hal.h"
namespace team04 {
class Stm32G4Resources final {
public:
    Stm32G4Resources() = default;
    Stm32G4Resources(const Stm32G4Resources&) = delete;
    Stm32G4Resources& operator=(const Stm32G4Resources&) = delete;
    SPI_HandleTypeDef sensorSpi{}, sdSpi{};
    DMA_HandleTypeDef sensorRxDma{}, sensorTxDma{};
    TIM_HandleTypeDef timebase{}, watchdogTimer{};
};
class Stm32G4Config final {
public:
    /* Call after HAL_Init; omit if the application already owns clocks. */
    static bool configureClock();
    /* Reserves SPI2, SPI3, TIM2, TIM6, DMA1 channels 1/2 and EXTI4. */
    static bool configureResources(Stm32G4Resources& resources);
    static void enableExti(bool enable);
    Stm32G4Config() = delete;
};
}
#endif
/***************************** END OF FILE ************************************/
