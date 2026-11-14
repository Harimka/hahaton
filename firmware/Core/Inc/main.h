/**
  ******************************************************************************
  * @file    main.h
  * @author  team-04
  * @brief   Header for main.cpp module.
  ******************************************************************************
  */

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/* Pin definitions according to COMPONENTS_INFO.md */
#define LED_PIN                  GPIO_PIN_5
#define LED_GPIO_PORT            GPIOA

/* IMU LSM6DS3 Pins (SPI2) */
#define IMU_CS_PIN               GPIO_PIN_12
#define IMU_CS_GPIO_PORT         GPIOB
#define IMU_SCK_PIN              GPIO_PIN_13
#define IMU_SCK_GPIO_PORT        GPIOB
#define IMU_MISO_PIN             GPIO_PIN_14
#define IMU_MISO_GPIO_PORT       GPIOB
#define IMU_MOSI_PIN             GPIO_PIN_15
#define IMU_MOSI_GPIO_PORT       GPIOB
#define IMU_INT1_PIN             GPIO_PIN_4
#define IMU_INT1_GPIO_PORT       GPIOC

/* MicroSD Card Pins (SPI3) */
#define SD_CS_PIN                GPIO_PIN_4
#define SD_CS_GPIO_PORT          GPIOA
#define SD_SCK_PIN               GPIO_PIN_10
#define SD_SCK_GPIO_PORT         GPIOC
#define SD_MISO_PIN              GPIO_PIN_11
#define SD_MISO_GPIO_PORT        GPIOC
#define SD_MOSI_PIN              GPIO_PIN_12
#define SD_MOSI_GPIO_PORT        GPIOC

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
/***************************** END OF FILE ************************************/
