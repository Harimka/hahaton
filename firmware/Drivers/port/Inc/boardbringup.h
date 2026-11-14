/** @file boardbringup.h
 * @author team-04
 * @brief Optional STM32G474 bring-up and IRQ forwarding helper.
 */
#ifndef TEAM04_BOARDBRINGUP_H_
#define TEAM04_BOARDBRINGUP_H_
#include "stm32g4config.h"
#include "stm32g4spi.h"
namespace team04 {
enum class BoardState : uint8_t { kStopped, kPowerWait, kResetWait, kSensorWait, kReady, kError };
class BoardBringup final {
public:
    BoardBringup(Stm32G4Resources& resources, Stm32G4Spi& sensorBus,
                 Stm32G4Clock& clock, Lsm6ds3& sensor, FifoCapture& capture);
    BoardBringup(const BoardBringup&) = delete;
    BoardBringup& operator=(const BoardBringup&) = delete;
    /* HAL_Init must already have run; startup owns clocks and all named resources. */
    bool start();
    BoardState poll();
    bool recoverFifo();
    void onExti(uint16_t pin);
    void onTimer(TIM_HandleTypeDef* timer);
private:
    Stm32G4Resources& resources_;
    Stm32G4Spi& sensorBus_;
    Stm32G4Clock& clock_;
    Lsm6ds3& sensor_;
    FifoCapture& capture_;
    BoardState state_;
    uint32_t phaseStartMs_;
    volatile bool timerReady_;
};
}
#endif
/***************************** END OF FILE ************************************/
