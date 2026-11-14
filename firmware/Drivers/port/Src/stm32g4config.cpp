/** @file stm32g4config.cpp
 * @author team-04
 * @brief Optional NUCLEO-G474RE resource setup for the sensor and SD.
 */
#include "stm32g4config.h"

namespace team04 {
bool Stm32G4Config::configureClock() {
    __HAL_RCC_PWR_CLK_ENABLE();
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK) return false;
    RCC_OscInitTypeDef osc{};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI; osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON; osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = RCC_PLLM_DIV4; osc.PLL.PLLN = 85U;
    osc.PLL.PLLP = RCC_PLLP_DIV2; osc.PLL.PLLQ = RCC_PLLQ_DIV2; osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) return false;
    RCC_ClkInitTypeDef clk{};
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2; clk.APB2CLKDivider = RCC_HCLK_DIV1;
    return HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) == HAL_OK;
}
bool Stm32G4Config::configureResources(Stm32G4Resources& r) {
    if (HAL_RCC_GetHCLKFreq() != 170000000U || HAL_RCC_GetPCLK1Freq() != 85000000U) return false;
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE(); __HAL_RCC_SPI3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE(); __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE(); __HAL_RCC_TIM6_CLK_ENABLE();
    HAL_NVIC_DisableIRQ(EXTI4_IRQn);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_12; gpio.Mode = GPIO_MODE_OUTPUT_PP; gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_4; HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15; gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF5_SPI2; HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12; gpio.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(GPIOC, &gpio);
    gpio.Pin = GPIO_PIN_4; gpio.Mode = GPIO_MODE_IT_RISING; gpio.Pull = GPIO_NOPULL; gpio.Alternate = 0U;
    HAL_GPIO_Init(GPIOC, &gpio);

    r.sensorRxDma = {}; r.sensorRxDma.Instance = DMA1_Channel1;
    r.sensorRxDma.Init.Request = DMA_REQUEST_SPI2_RX; r.sensorRxDma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    r.sensorRxDma.Init.PeriphInc = DMA_PINC_DISABLE; r.sensorRxDma.Init.MemInc = DMA_MINC_ENABLE;
    r.sensorRxDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE; r.sensorRxDma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    r.sensorRxDma.Init.Mode = DMA_NORMAL; r.sensorRxDma.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&r.sensorRxDma) != HAL_OK) return false;
    r.sensorTxDma = {}; r.sensorTxDma.Instance = DMA1_Channel2;
    r.sensorTxDma.Init = r.sensorRxDma.Init;
    r.sensorTxDma.Init.Request = DMA_REQUEST_SPI2_TX; r.sensorTxDma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    if (HAL_DMA_Init(&r.sensorTxDma) != HAL_OK) return false;

    r.sensorSpi = {}; r.sensorSpi.Instance = SPI2;
    r.sensorSpi.Init.Mode = SPI_MODE_MASTER; r.sensorSpi.Init.Direction = SPI_DIRECTION_2LINES;
    r.sensorSpi.Init.DataSize = SPI_DATASIZE_8BIT; r.sensorSpi.Init.CLKPolarity = SPI_POLARITY_HIGH;
    r.sensorSpi.Init.CLKPhase = SPI_PHASE_2EDGE; r.sensorSpi.Init.NSS = SPI_NSS_SOFT;
    r.sensorSpi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; r.sensorSpi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    r.sensorSpi.Init.TIMode = SPI_TIMODE_DISABLE; r.sensorSpi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    r.sensorSpi.Init.CRCPolynomial = 7U; r.sensorSpi.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    r.sensorSpi.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    __HAL_LINKDMA(&r.sensorSpi, hdmarx, r.sensorRxDma);
    __HAL_LINKDMA(&r.sensorSpi, hdmatx, r.sensorTxDma);
    if (HAL_SPI_Init(&r.sensorSpi) != HAL_OK) return false;
    r.sdSpi = {}; r.sdSpi.Instance = SPI3; r.sdSpi.Init = r.sensorSpi.Init;
    r.sdSpi.Init.CLKPolarity = SPI_POLARITY_LOW; r.sdSpi.Init.CLKPhase = SPI_PHASE_1EDGE;
    r.sdSpi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    if (HAL_SPI_Init(&r.sdSpi) != HAL_OK) return false;

    r.timebase = {}; r.timebase.Instance = TIM2; r.timebase.Init.Prescaler = 169U;
    r.timebase.Init.CounterMode = TIM_COUNTERMODE_UP; r.timebase.Init.Period = 0xffffffffU;
    r.timebase.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; r.timebase.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&r.timebase) != HAL_OK) return false;
    r.watchdogTimer = {}; r.watchdogTimer.Instance = TIM6;
    r.watchdogTimer.Init.Prescaler = 169U; r.watchdogTimer.Init.CounterMode = TIM_COUNTERMODE_UP;
    r.watchdogTimer.Init.Period = 999U; r.watchdogTimer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&r.watchdogTimer) != HAL_OK) return false;
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5U, 0U); HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 5U, 0U); HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    HAL_NVIC_SetPriority(SPI2_IRQn, 5U, 0U); HAL_NVIC_EnableIRQ(SPI2_IRQn);
    HAL_NVIC_SetPriority(EXTI4_IRQn, 6U, 0U);
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 7U, 0U); HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    return true;
}
void Stm32G4Config::enableExti(bool enable) {
    if (enable) { __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4); HAL_NVIC_EnableIRQ(EXTI4_IRQn); }
    else HAL_NVIC_DisableIRQ(EXTI4_IRQn);
}
}
/***************************** END OF FILE ************************************/
