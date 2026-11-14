/**
  ******************************************************************************
  * @file    stm32g4xx_hal_conf.h
  * @author  team-04
  * @brief   HAL configuration file for STM32G474RE Nucleo.
  ******************************************************************************
  */

#ifndef STM32G4xx_HAL_CONF_H
#define STM32G4xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Module Selection */
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* Oscillator Values adaptation */
#if !defined(HSI_VALUE)
  #define HSI_VALUE           (16000000UL) /*!< Value of the Internal oscillator in Hz */
#endif

#if !defined(HSI48_VALUE)
  #define HSI48_VALUE         (48000000UL) /*!< Value of the Internal High Speed oscillator for USB FS/RNG */
#endif

#if !defined(HSE_VALUE)
  #define HSE_VALUE           (24000000UL) /*!< Value of the External oscillator in Hz */
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT (100UL)      /*!< Time out for HSE start up, in ms */
#endif

#if !defined(LSE_VALUE)
  #define LSE_VALUE           (32768UL)    /*!< Value of the External oscillator in Hz */
#endif

#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT (5000UL)     /*!< Time out for LSE start up, in ms */
#endif

#if !defined(LSI_VALUE)
  #define LSI_VALUE           (32000UL)    /*!< Value of the Internal Low Speed oscillator in Hz */
#endif

#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE (48000UL)   /*!< Value of the External clock source in Hz */
#endif

#if !defined(VDD_VALUE)
  #define VDD_VALUE           (3300UL)     /*!< Value of VDD in mv */
#endif

#if !defined(PREFETCH_ENABLE)
  #define PREFETCH_ENABLE     1U
#endif

#if !defined(INSTRUCTION_CACHE_ENABLE)
  #define INSTRUCTION_CACHE_ENABLE 1U
#endif

#if !defined(DATA_CACHE_ENABLE)
  #define DATA_CACHE_ENABLE   1U
#endif

#define TICK_INT_PRIORITY     (0x0FU)      /*!< tick interrupt priority */
#define USE_RTOS              0U

/* Includes */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32g4xx_hal_rcc.h"
  #include "stm32g4xx_hal_rcc_ex.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32g4xx_hal_gpio.h"
  #include "stm32g4xx_hal_gpio_ex.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32g4xx_hal_dma.h"
  #include "stm32g4xx_hal_dma_ex.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32g4xx_hal_cortex.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32g4xx_hal_flash.h"
  #include "stm32g4xx_hal_flash_ex.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32g4xx_hal_pwr.h"
  #include "stm32g4xx_hal_pwr_ex.h"
#endif

#ifdef HAL_SPI_MODULE_ENABLED
  #include "stm32g4xx_hal_spi.h"
  #include "stm32g4xx_hal_spi_ex.h"
#endif

#ifdef HAL_TIM_MODULE_ENABLED
  #include "stm32g4xx_hal_tim.h"
  #include "stm32g4xx_hal_tim_ex.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32g4xx_hal_uart.h"
  #include "stm32g4xx_hal_uart_ex.h"
#endif

#ifdef HAL_EXTI_MODULE_ENABLED
  #include "stm32g4xx_hal_exti.h"
#endif

/* Exported macro ------------------------------------------------------------*/
#ifdef  USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32G4xx_HAL_CONF_H */
/***************************** END OF FILE ************************************/
