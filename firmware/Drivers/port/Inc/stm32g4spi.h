/** @file stm32g4spi.h
 * @author team-04
 * @brief STM32G4 implementations of the SPI, clock, and critical-section ports.
 */
#ifndef TEAM04_STM32G4SPI_H_
#define TEAM04_STM32G4SPI_H_
#include "stm32g4xx_hal.h"
#include "fifocapture.h"
namespace team04 {
class Stm32G4Spi final : public SpiDrv {
public:
    Stm32G4Spi(SPI_HandleTypeDef& spi, GPIO_TypeDef& csPort, uint16_t csPin);
    Stm32G4Spi(const Stm32G4Spi&) = delete;
    Stm32G4Spi& operator=(const Stm32G4Spi&) = delete;
    IoStatus setClock(uint32_t maxHz) override;
    void select() override;
    void deselect() override;
    IoStatus transfer(const uint8_t* tx, uint8_t* rx, uint16_t len, uint32_t timeoutMs) override;
    IoStatus startDma(const uint8_t* tx, uint8_t* rx, uint16_t len) override;
    IoStatus abortDma() override;
    void attachCapture(FifoCapture& capture);
    void onComplete(SPI_HandleTypeDef* spi);
    void onError(SPI_HandleTypeDef* spi);
    void onAbortComplete(SPI_HandleTypeDef* spi);
    void service(uint32_t timeoutUs);
private:
    SPI_HandleTypeDef& spi_;
    GPIO_TypeDef& csPort_;
    uint16_t csPin_;
    FifoCapture* capture_;
    volatile bool abortCompleted_;
};
class Stm32G4Clock final : public ClockDrv {
public:
    explicit Stm32G4Clock(TIM_HandleTypeDef& timer);
    Stm32G4Clock(const Stm32G4Clock&) = delete;
    Stm32G4Clock& operator=(const Stm32G4Clock&) = delete;
    uint32_t millis() const override;
    uint32_t micros() const override;
private:
    TIM_HandleTypeDef& timer_;
};
class Stm32G4Critical final : public CriticalSection {
public:
    Stm32G4Critical() = default;
    Stm32G4Critical(const Stm32G4Critical&) = delete;
    Stm32G4Critical& operator=(const Stm32G4Critical&) = delete;
    uint32_t enter() override;
    void leave(uint32_t state) override;
};
}
#endif
/***************************** END OF FILE ************************************/
