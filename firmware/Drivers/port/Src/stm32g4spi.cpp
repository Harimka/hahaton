/** @file stm32g4spi.cpp
 * @author team-04
 * @brief HAL SPI/DMA adapter; application forwards HAL callbacks explicitly.
 */
#include "stm32g4spi.h"
namespace team04 {
namespace {
IoStatus convert(HAL_StatusTypeDef s) {
    switch (s) { case HAL_OK: return IoStatus::kOk; case HAL_BUSY: return IoStatus::kBusy;
        case HAL_TIMEOUT: return IoStatus::kTimeout; default: return IoStatus::kError; }
}
}
Stm32G4Spi::Stm32G4Spi(SPI_HandleTypeDef& spi, GPIO_TypeDef& csPort, uint16_t csPin)
    : spi_(spi)
    , csPort_(csPort)
    , csPin_(csPin)
    , capture_(nullptr)
    , abortCompleted_(false)
{
}
void Stm32G4Spi::select() { HAL_GPIO_WritePin(&csPort_, csPin_, GPIO_PIN_RESET); }
void Stm32G4Spi::deselect() { HAL_GPIO_WritePin(&csPort_, csPin_, GPIO_PIN_SET); }
IoStatus Stm32G4Spi::setClock(uint32_t maxHz) {
    if (!maxHz || HAL_SPI_GetState(&spi_) != HAL_SPI_STATE_READY) return IoStatus::kBusy;
    const uint32_t values[] = {SPI_BAUDRATEPRESCALER_2,SPI_BAUDRATEPRESCALER_4,SPI_BAUDRATEPRESCALER_8,
        SPI_BAUDRATEPRESCALER_16,SPI_BAUDRATEPRESCALER_32,SPI_BAUDRATEPRESCALER_64,
        SPI_BAUDRATEPRESCALER_128,SPI_BAUDRATEPRESCALER_256};
    const uint32_t pclk = HAL_RCC_GetPCLK1Freq(); uint32_t div = 2U; uint8_t i = 0U;
    while (i < 8U && static_cast<uint64_t>(maxHz) * div < pclk) { ++i; div *= 2U; }
    if (i == 8U) return IoStatus::kUnsupported;
    __HAL_SPI_DISABLE(&spi_); MODIFY_REG(spi_.Instance->CR1, SPI_CR1_BR, values[i]);
    spi_.Init.BaudRatePrescaler = values[i]; return IoStatus::kOk;
}
IoStatus Stm32G4Spi::transfer(const uint8_t* tx, uint8_t* rx, uint16_t len, uint32_t timeoutMs) {
    if (!len || !timeoutMs) return IoStatus::kInvalid;
    if (tx && rx) return convert(HAL_SPI_TransmitReceive(&spi_, tx, rx, len, timeoutMs));
    if (tx) return convert(HAL_SPI_Transmit(&spi_, tx, len, timeoutMs));
    uint8_t dummy[32]; for (uint8_t& b : dummy) b = 0xffU;
    const uint32_t start = HAL_GetTick();
    while (len) {
        const uint32_t elapsed = HAL_GetTick() - start;
        if (elapsed >= timeoutMs) return IoStatus::kTimeout;
        const uint16_t n = len > sizeof(dummy) ? sizeof(dummy) : len;
        const auto status = rx ? HAL_SPI_TransmitReceive(&spi_, dummy, rx, n, timeoutMs - elapsed)
                               : HAL_SPI_Transmit(&spi_, dummy, n, timeoutMs - elapsed);
        if (status != HAL_OK) return convert(status);
        if (rx) rx += n;
        len = static_cast<uint16_t>(len - n);
    }
    return IoStatus::kOk;
}
IoStatus Stm32G4Spi::startDma(const uint8_t* tx, uint8_t* rx, uint16_t len) {
    if (!tx || !rx || !len || !spi_.hdmarx || !spi_.hdmatx) return IoStatus::kInvalid;
    return convert(HAL_SPI_TransmitReceive_DMA(&spi_, tx, rx, len));
}
IoStatus Stm32G4Spi::abortDma() {
    abortCompleted_ = false;
    /* SPI_DMAError can clear SPI DMA-request bits before the error callback,
     * while the other DMA channel is still enabled. Quiesce both channels
     * explicitly before asking HAL to reset SPI state. HAL_DMA_Abort is a
     * register-only operation on STM32G4 and does not wait for a tick. */
    CLEAR_BIT(spi_.Instance->CR2, SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);
    DMA_HandleTypeDef* channels[] = {spi_.hdmarx, spi_.hdmatx};
    for (DMA_HandleTypeDef* channel : channels) {
        if (!channel) return IoStatus::kError;
        if (channel->State == HAL_DMA_STATE_BUSY && HAL_DMA_Abort(channel) != HAL_OK) return IoStatus::kError;
        if ((channel->Instance->CCR & DMA_CCR_EN) != 0U) return IoStatus::kError;
    }
    return convert(HAL_SPI_Abort_IT(&spi_));
}
void Stm32G4Spi::attachCapture(FifoCapture& capture) { capture_ = &capture; }
void Stm32G4Spi::onComplete(SPI_HandleTypeDef* spi) {
    if (spi == &spi_ && capture_) { deselect(); capture_->onDmaComplete(true); }
}
void Stm32G4Spi::onError(SPI_HandleTypeDef* spi) {
    if (spi == &spi_ && capture_) capture_->onDmaComplete(false);
}
void Stm32G4Spi::onAbortComplete(SPI_HandleTypeDef* spi) {
    if (spi == &spi_ && capture_) { deselect(); abortCompleted_ = true; }
}
void Stm32G4Spi::service(uint32_t timeoutUs) {
    if (!capture_) return;
    const uint32_t state = __get_PRIMASK(); __disable_irq();
    const bool completed = abortCompleted_ && __HAL_SPI_GET_FLAG(&spi_, SPI_FLAG_BSY) == RESET;
    if (completed) abortCompleted_ = false;
    if (!(state & 1U)) __enable_irq();
    if (completed) capture_->onAbortComplete();
    capture_->watchdog(timeoutUs);
}
Stm32G4Clock::Stm32G4Clock(TIM_HandleTypeDef& timer)
    : timer_(timer)
{
}
uint32_t Stm32G4Clock::millis() const { return HAL_GetTick(); }
uint32_t Stm32G4Clock::micros() const { return __HAL_TIM_GET_COUNTER(&timer_); }
uint32_t Stm32G4Critical::enter() { const uint32_t state = __get_PRIMASK(); __disable_irq(); return state; }
void Stm32G4Critical::leave(uint32_t state) { if (!(state & 1U)) __enable_irq(); }
}
/***************************** END OF FILE ************************************/
