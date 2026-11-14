/**
  ******************************************************************************
  * @file    main.cpp
  * @author  team-04
  * @brief   Main program body for STM32G474RE Nucleo board.
  *          Features:
  *          - Maximum performance configuration (170 MHz Boost Mode, FPU)
  *          - Pinout initialization (LSM6DS3, SD Card, LED)
  *          - Non-blocking LED test pattern: 1 long blink, 2 short blinks
  ******************************************************************************
  */

#include "main.h"

namespace {

/**
 * @brief LED blink step structure for pattern sequence.
 */
struct BlinkStep {
    bool state;         ///< true = LED ON, false = LED OFF
    uint32_t durationMs;///< Duration in milliseconds
};

/**
 * @brief Pattern: 1 Long Blink, 2 Short Blinks, Pause
 * - Long ON: 800 ms
 * - OFF gap: 300 ms
 * - Short 1 ON: 200 ms
 * - OFF gap: 200 ms
 * - Short 2 ON: 200 ms
 * - Repeat Pause OFF: 1000 ms
 */
static constexpr BlinkStep kBlinkPattern[] = {
    {true,  800},
    {false, 300},
    {true,  200},
    {false, 200},
    {true,  200},
    {false, 1000}
};

static constexpr uint32_t kPatternLength = sizeof(kBlinkPattern) / sizeof(kBlinkPattern[0]);

} // namespace

/* Private function prototypes */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* Enable Hardware Floating Point Unit (FPU) */
    #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
        SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));
        __DSB();
        __ISB();
    #endif

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock to maximum 170 MHz in Range 1 Boost mode */
    SystemClock_Config();

    /* Initialize all configured GPIO pins */
    MX_GPIO_Init();

    /* Ensure Chip Selects are inactive (HIGH) */
    HAL_GPIO_WritePin(IMU_CS_GPIO_PORT, IMU_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_SET);

    /* Blink state machine variables */
    uint32_t currentStep = 0;
    uint32_t lastStepTick = HAL_GetTick();

    /* Apply initial step */
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, 
                      kBlinkPattern[currentStep].state ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* Infinite superloop (Non-blocking) */
    while (1)
    {
        uint32_t currentTick = HAL_GetTick();
        if ((currentTick - lastStepTick) >= kBlinkPattern[currentStep].durationMs)
        {
            lastStepTick = currentTick;
            currentStep = (currentStep + 1) % kPatternLength;

            HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, 
                              kBlinkPattern[currentStep].state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        /* Further background tasks / IMU poll will be called here without blocking */
    }
}

/**
  * @brief System Clock Configuration
  *        Configures clock tree for maximum speed: 170 MHz SYSCLK
  *        HSI 16 MHz -> PLLM /4 -> PLLN *85 -> PLLR /2 = 170 MHz
  *        PWR Range 1 Boost Mode enabled for > 150 MHz operation.
  * @retval None
  */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Enable Power Control clock */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* Configure Voltage Scaling to Range 1 Boost (Required for 170 MHz) */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    /* Enable ART Accelerator / Flash Caches for maximum execution speed */
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();

    /* Configure HSI Oscillator and PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;     /* 16 MHz / 4 = 4 MHz */
    RCC_OscInitStruct.PLL.PLLN = 85;               /* 4 MHz * 85 = 340 MHz VCO */
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;     /* 340 MHz / 2 = 170 MHz SYSCLK */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* Initialize CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;  /* HCLK = 170 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;   /* PCLK1 = 85 MHz (for SPI3 SD init < 400kHz) */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;   /* PCLK2 = 170 MHz */

    /* 4 Wait States required at 170 MHz with Range 1 Boost */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Configure LD2 LED Pin (PA5) */
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    /* Configure IMU Chip Select (PB12) - Active LOW, default HIGH */
    HAL_GPIO_WritePin(IMU_CS_GPIO_PORT, IMU_CS_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = IMU_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(IMU_CS_GPIO_PORT, &GPIO_InitStruct);

    /* Configure SD Card Chip Select (PA4) - Active LOW, default HIGH */
    HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = SD_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(SD_CS_GPIO_PORT, &GPIO_InitStruct);

    /* Configure IMU INT1 Pin (PC4) as Input */
    GPIO_InitStruct.Pin = IMU_INT1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(IMU_INT1_GPIO_PORT, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        /* Fatal error trap */
    }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif /* USE_FULL_ASSERT */

/***************************** END OF FILE ************************************/
