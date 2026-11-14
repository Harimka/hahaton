/** @file boardbringup.cpp
 * @author team-04
 * @brief Nonblocking sensor reset and FIFO initialization for STM32G474.
 */
#include "boardbringup.h"
namespace team04 {
BoardBringup::BoardBringup(Stm32G4Resources& resources, Stm32G4Spi& sensorBus,
                           Stm32G4Clock& clock, Lsm6ds3& sensor, FifoCapture& capture)
    : resources_(resources)
    , sensorBus_(sensorBus)
    , clock_(clock)
    , sensor_(sensor)
    , capture_(capture)
    , state_(BoardState::kStopped)
    , phaseStartMs_(0U)
    , timerReady_(false)
{
}
bool BoardBringup::start() {
    state_ = BoardState::kError; timerReady_ = false;
    if (!Stm32G4Config::configureClock() || !Stm32G4Config::configureResources(resources_) ||
        HAL_TIM_Base_Start(&resources_.timebase) != HAL_OK) return false;
    sensorBus_.deselect(); sensorBus_.attachCapture(capture_);
    phaseStartMs_ = clock_.millis(); state_ = BoardState::kPowerWait; timerReady_ = true;
    if (HAL_TIM_Base_Start_IT(&resources_.watchdogTimer) != HAL_OK) {
        timerReady_ = false; state_ = BoardState::kError; return false;
    }
    return true;
}
BoardState BoardBringup::poll() {
    if (state_ == BoardState::kPowerWait && static_cast<uint32_t>(clock_.millis() - phaseStartMs_) >= 15U) {
        if (sensor_.resetBegin() != IoStatus::kOk) return state_ = BoardState::kError;
        phaseStartMs_ = clock_.millis(); state_ = BoardState::kResetWait;
    }
    if (state_ == BoardState::kResetWait) {
        bool complete = false;
        if (sensor_.resetPoll(complete) != IoStatus::kOk ||
            (!complete && static_cast<uint32_t>(clock_.millis() - phaseStartMs_) >= 100U))
            return state_ = BoardState::kError;
        if (complete) {
            if (sensor_.configure() != IoStatus::kOk) return state_ = BoardState::kError;
            phaseStartMs_ = clock_.millis(); state_ = BoardState::kSensorWait;
        }
    }
    /* Allow the first four 6.66 kHz output samples to settle before enabling FIFO. */
    if (state_ == BoardState::kSensorWait && static_cast<uint32_t>(clock_.millis() - phaseStartMs_) >= 2U) {
            if (sensor_.configureFifo(FifoCapture::kBatchSamples) != IoStatus::kOk ||
                sensor_.enableFifoInterrupt(true) != IoStatus::kOk) return state_ = BoardState::kError;
            state_ = BoardState::kReady; Stm32G4Config::enableExti(true);
            /* Handle a watermark that became active before EXTI was enabled. */
            capture_.kick();
    }
    return state_;
}
bool BoardBringup::recoverFifo() {
    if (state_ != BoardState::kReady || !capture_.canRecover()) return false;
    Stm32G4Config::enableExti(false);
    if (sensor_.configureFifo(FifoCapture::kBatchSamples) != IoStatus::kOk || !capture_.rearm() ||
        sensor_.enableFifoInterrupt(true) != IoStatus::kOk) return false;
    Stm32G4Config::enableExti(true); capture_.kick(); return true;
}
void BoardBringup::onExti(uint16_t pin) { if (pin == GPIO_PIN_4) capture_.onWatermark(); }
void BoardBringup::onTimer(TIM_HandleTypeDef* timer) {
    if (timerReady_ && timer == &resources_.watchdogTimer) sensorBus_.service(5000U);
}
}
/***************************** END OF FILE ************************************/
