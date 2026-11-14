/** @file boardintegration.cpp
 * @author team-04
 * @brief Optional integration example; merge callbacks into the owning project.
 */
#include "boardbringup.h"
#include "fatfsadapter.h"
#include "sdcard.h"
using namespace team04;
namespace {
/* Main SRAM: do not place these DMA buffers in CCM. */
Stm32G4Resources resources;
Stm32G4Spi sensorBus(resources.sensorSpi, *GPIOB, GPIO_PIN_12);
Stm32G4Spi sdBus(resources.sdSpi, *GPIOA, GPIO_PIN_4);
Stm32G4Clock clockPort(resources.timebase);
Stm32G4Critical critical;
Lsm6ds3 sensor(sensorBus);
FifoCapture capture(sensorBus, clockPort, critical);
SdCard card(sdBus, clockPort);
FatFsAdapter fatfs(card);
BoardBringup board(resources, sensorBus, clockPort, sensor, capture);
FATFS filesystem;
char sdPath[4];
bool linked = false;
}
/* HAL_Init() is called by the application before this function. */
extern "C" bool exampleBoardStart() { return board.start(); }
extern "C" BoardState exampleBoardPoll() { return board.poll(); }
extern "C" bool exampleRecoverFifo() { return board.recoverFifo(); }
/* Mount from the main loop, never from an IRQ. Do not re-link while mounted. */
extern "C" FRESULT exampleMountSd() {
    if (!linked) {
        if (!fatfs.bind() || FATFS_LinkDriver(&FatFsAdapter::kDriver, sdPath) != 0U) return FR_NOT_READY;
        linked = true;
    }
    return f_mount(&filesystem, sdPath, 1U);
}
/* A single consumer copies/uses the block before calling exampleReleaseBlock. */
extern "C" const CaptureBlock* examplePeekBlock() { return capture.peek(); }
extern "C" void exampleReleaseBlock() { capture.release(); }
extern "C" CaptureStats exampleCaptureStats() { return capture.snapshot(); }
/* Forward from the application's existing IRQ handlers and HAL callbacks.
 * If this example is compiled, do not define duplicate handlers elsewhere. */
extern "C" void EXTI4_IRQHandler() { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4); }
extern "C" void DMA1_Channel1_IRQHandler() { HAL_DMA_IRQHandler(&resources.sensorRxDma); }
extern "C" void DMA1_Channel2_IRQHandler() { HAL_DMA_IRQHandler(&resources.sensorTxDma); }
extern "C" void SPI2_IRQHandler() { HAL_SPI_IRQHandler(&resources.sensorSpi); }
extern "C" void TIM6_DAC_IRQHandler() { HAL_TIM_IRQHandler(&resources.watchdogTimer); }
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t pin) { board.onExti(pin); }
extern "C" void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* spi) { sensorBus.onComplete(spi); }
extern "C" void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* spi) { sensorBus.onError(spi); }
extern "C" void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef* spi) { sensorBus.onAbortComplete(spi); }
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* timer) { board.onTimer(timer); }
/***************************** END OF FILE ************************************/
