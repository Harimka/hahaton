/**
 * @file    main.cpp
 * @author  team-04
 * @brief   Main application for STM32G474RE Nucleo board with LSM6DS3 IMU telemetry.
 *          Features:
 *          - 170 MHz SYSCLK (Boost mode) + Hardware FPU
 *          - SPI2 Master @ 5.31 MHz for LSM6DS3 Accelerometer + Gyroscope
 *          - LPUART1 @ 921600 8N1 over ST-LINK V3 Virtual COM Port (PA2 TX / PA3 RX)
 *          - Non-blocking superloop streaming calibrated 6-axis IMU data to UART
 *          - Heartbeat LED toggling on PA5
 */

#include "main.h"
#include <stdio.h>
#include <cstring>
#include "lsm6ds3.h"
#include "stm32g4spi.h"
#include "stm32g4uart.h"
#include "sdcard.h"
#include "fatfsadapter.h"
#include "sample_ring_buffer.h"
#include "shock_analyzer.h"
#include "ff.h"
#include "ff_gen_drv.h"

/* Peripheral Handles */
SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;
UART_HandleTypeDef hlpuart1;

namespace {
class MainClock final : public team04::ClockDrv {
public:
    ~MainClock() override = default;
    uint32_t millis() const override { return HAL_GetTick(); }
    uint32_t micros() const override
    {
        const uint32_t cyclesPerMicrosecond = SystemCoreClock / 1000000U;
        return cyclesPerMicrosecond == 0U ? 0U : DWT->CYCCNT / cyclesPerMicrosecond;
    }
};

static void EnableHighResolutionClock(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

}

/* Private function prototypes */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI3_Init(void);

/**
 * @brief Standard library syscalls stubs and _write redirection to LPUART1.
 */
extern "C" {

int _write(int file, char *ptr, int len)
{
    (void)file;
    if (ptr && len > 0) {
        HAL_UART_Transmit(&hlpuart1, reinterpret_cast<uint8_t*>(ptr), static_cast<uint16_t>(len), 100U);
    }
    return len;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, void *st)
{
    (void)file;
    (void)st;
    return -1;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return -1;
}

} // extern "C"

/**
 * @brief The application entry point.
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
    EnableHighResolutionClock();

    /* Initialize all configured GPIO pins */
    MX_GPIO_Init();

    /* Initialize ST-LINK V3 Virtual COM Port (LPUART1) */
    MX_LPUART1_UART_Init();

    /* Initialize SPI2 for LSM6DS3 IMU */
    MX_SPI2_Init();
    /* Initialize SPI3 for the SD card */
    MX_SPI3_Init();

    /* Ensure Chip Selects are inactive (HIGH) */
    HAL_GPIO_WritePin(IMU_CS_GPIO_PORT, IMU_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_SET);

    /* Construct BSP and Port driver objects statically */
    team04::Stm32G4Spi spiBus(hspi2, *IMU_CS_GPIO_PORT, IMU_CS_PIN);
    team04::Stm32G4Spi sdBus(hspi3, *SD_CS_GPIO_PORT, SD_CS_PIN);
    team04::Stm32G4Uart uartPort(hlpuart1);
    team04::Lsm6ds3 imu(spiBus);
    MainClock sdClock;
    team04::SdCard sdCard(sdBus, sdClock);
    team04::FatFsAdapter sdAdapter(sdCard);
    FATFS sdFs{};
    FIL sdFile{};

    /* Print initial startup banner to UART */
    uartPort.print("\r\n");
    uartPort.print("====================================================================\r\n");
    uartPort.print(" [Team-04] STM32G474RE Nucleo: Direct high-rate accelerometer capture\r\n");
    uartPort.print(" SYSCLK: 170 MHz | SPI2: 5.31 MHz | UART: LPUART1 921600 8N1\r\n");
    uartPort.print(" Capture: XLDA polling, no FIFO, static 2048-sample RAM ring\r\n");
    uartPort.print("====================================================================\r\n\r\n");

    /* Probe LSM6DS3 */
    uartPort.print("[INIT] Probing LSM6DS3 on SPI2 (CS: PB12, SCK: PB13, MISO: PB14, MOSI: PB15)...\r\n");
    uint8_t chipId = 0U;
    team04::IoStatus status = imu.probe(chipId);
    uartPort.printf("[INIT] WHO_AM_I register = 0x%02X (Expected: 0x69)\r\n", chipId);

    if (status == team04::IoStatus::kOk) {
        uartPort.print("[INIT] LSM6DS3 communication verified!\r\n");
        status = imu.configure();
        if (status == team04::IoStatus::kOk) {
            uartPort.print("[INIT] LSM6DS3 configured: Accel 6.66 kHz +-16g | Gyro 6.66 kHz +-2000 dps\r\n");
        } else {
            uartPort.print("[ERROR] Failed to configure LSM6DS3 registers!\r\n");
        }
    } else {
        uartPort.printf("[ERROR] LSM6DS3 not detected (Status code: %u)!\r\n",
                        static_cast<unsigned>(status));
        uartPort.print("[HINT] Check wiring: PB12=CS, PB13=SCK, PB14=SDO, PB15=SDI, 3.3V, GND\r\n");
    }

    uartPort.print("[SD] Initializing SPI3 SD card...\r\n");
    const bool sdBound = sdAdapter.bind();
    const uint8_t driverLinked = sdBound
        ? FATFS_LinkDriver(&team04::FatFsAdapter::kDriver, "0:")
        : 1U;
    const FRESULT mountResult = (driverLinked == 0U)
        ? f_mount(&sdFs, "0:", 1)
        : FR_NOT_READY;
    const char* const sdTestPath = "0:/SDTEST.TXT";
    const char* const sdTestPayload = "team04-sd-smoke-test-v1\r\n";
    const UINT sdTestLength = static_cast<UINT>(strlen(sdTestPayload));
    FRESULT sdTestResult = mountResult;
    bool sdTestPassed = false;

    if (sdBound && driverLinked == 0U && mountResult == FR_OK) {
        UINT bytesWritten = 0U;
        UINT bytesRead = 0U;
        char readback[sizeof("team04-sd-smoke-test-v1\r\n")] = {};

        sdTestResult = f_open(&sdFile, sdTestPath, FA_CREATE_ALWAYS | FA_WRITE);
        if (sdTestResult == FR_OK) {
            sdTestResult = f_write(&sdFile, sdTestPayload, sdTestLength, &bytesWritten);
            if (sdTestResult == FR_OK && bytesWritten != sdTestLength) {
                sdTestResult = FR_DISK_ERR;
            }
            if (sdTestResult == FR_OK) {
                sdTestResult = f_sync(&sdFile);
            }
            const FRESULT closeResult = f_close(&sdFile);
            if (sdTestResult == FR_OK) {
                sdTestResult = closeResult;
            }
        }

        if (sdTestResult == FR_OK) {
            sdTestResult = f_open(&sdFile, sdTestPath, FA_READ);
        }
        if (sdTestResult == FR_OK) {
            sdTestResult = f_read(&sdFile, readback, sdTestLength, &bytesRead);
            if (sdTestResult == FR_OK && bytesRead != sdTestLength) {
                sdTestResult = FR_INT_ERR;
            }
            if (sdTestResult == FR_OK && memcmp(readback, sdTestPayload, sdTestLength) != 0) {
                sdTestResult = FR_INT_ERR;
            }
            const FRESULT closeResult = f_close(&sdFile);
            if (sdTestResult == FR_OK) {
                sdTestResult = closeResult;
            }
        }

        sdTestPassed = sdTestResult == FR_OK;
    }

    if (sdTestPassed) {
        uartPort.printf("[SD TEST] PASS: wrote and verified %u bytes in SDTEST.TXT\r\n",
                        sdTestLength);
    } else {
        uartPort.printf("[SD TEST] FAIL: bind=%u link=%u mount=%u result=%u\r\n",
                        sdBound ? 1U : 0U,
                        driverLinked,
                        static_cast<unsigned>(mountResult),
                        static_cast<unsigned>(sdTestResult));
        uartPort.printf("[SD DIAG] init_stage=%u ready=%u block_addressed=%u last_response=0x%02X last_data=0x%02X spi_errors=%lu disk_errors=%lu\r\n",
                        static_cast<unsigned>(sdCard.initStage()),
                        sdCard.isBlockAddressed() ? 1U : 0U,
                        sdCard.isReady() ? 1U : 0U,
                        sdCard.lastResponse(),
                        sdCard.lastDataResponse(),
                        static_cast<unsigned long>(sdCard.transferErrorCount()),
                        static_cast<unsigned long>(sdAdapter.errorCount()));
    }

    uartPort.print("\r\n[CAPTURE] Shock detector active; SD output is mirrored as CSV to UART.\r\n");
    uartPort.print("[UART FORMAT] SHOCK_BEGIN/SHOCK_SAMPLE/SHOCK_END/SRS\r\n");

    static team04::SampleRingBuffer captureBuffer;
    static team04::ShockAnalyzer shockAnalyzer;
    static team04::ShockFrame shockFrame;
    static team04::SrsResult srsResult;
    uint32_t sampleCounter = 0U;
    uint32_t readErrorCounter = 0U;
    uint32_t lastHeartbeatTick = HAL_GetTick();

    /* High-rate path: poll XLDA, read six accelerometer bytes, store in static RAM. */
    while (1)
    {
        team04::AccelRawData accel{};
        const team04::IoStatus readStatus = imu.readAccelIfReady(accel);

        if (readStatus == team04::IoStatus::kOk) {
            const team04::CaptureSample sample = {
                sdClock.micros(),
                sampleCounter++,
                accel.x,
                accel.y,
                accel.z,
                0U
            };
            captureBuffer.push(sample);
            shockAnalyzer.process(sample, captureBuffer);
            if (shockAnalyzer.takeResult(shockFrame, srsResult)) {
                uartPort.printf("SHOCK_BEGIN;event=%lu;trigger_us=%lu;samples=%u\r\n",
                                static_cast<unsigned long>(shockFrame.eventId),
                                static_cast<unsigned long>(shockFrame.triggerTimestampUs),
                                shockFrame.count);
                for (uint16_t i = 0U; i < shockFrame.count; ++i) {
                    const team04::ShockSample& item = shockFrame.samples[i];
                    uartPort.printf("SHOCK_SAMPLE;%lu;%.6f;%.6f;%.6f\r\n",
                                    static_cast<unsigned long>(item.timestampUs),
                                    static_cast<double>(item.accelX),
                                    static_cast<double>(item.accelY),
                                    static_cast<double>(item.accelZ));
                }
                uartPort.printf("SHOCK_END;event=%lu;samples=%u\r\n",
                                static_cast<unsigned long>(shockFrame.eventId), shockFrame.count);
                for (uint16_t i = 0U; i < team04::SrsResult::kPointCount; ++i) {
                    const team04::SrsPoint& point = srsResult.points[i];
                    uartPort.printf("SRS;%.3f;%.6f;%.6f;%.6f\r\n",
                                    static_cast<double>(point.frequencyHz),
                                    static_cast<double>(point.gx),
                                    static_cast<double>(point.gy),
                                    static_cast<double>(point.gz));
                }
            }
        } else if (readStatus != team04::IoStatus::kNotReady) {
            ++readErrorCounter;
        }

        /* Diagnostic heartbeat only; it does not transmit and does not block sampling. */
        const uint32_t now = HAL_GetTick();
        if ((now - lastHeartbeatTick) >= 500U) {
            HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
            lastHeartbeatTick = now;
        }
    }
}

/**
 * @brief System Clock Configuration
 *        Configures clock tree for maximum speed: 170 MHz SYSCLK
 *        HSI 16 MHz -> PLLM /4 -> PLLN *85 -> PLLR /2 = 170 MHz
 *        PWR Range 1 Boost Mode enabled for > 150 MHz operation.
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;   /* PCLK1 = 85 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;   /* PCLK2 = 170 MHz */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }

    /* Select PCLK1 as LPUART1 clock source */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
    PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief LPUART1 Initialization Function (ST-LINK V3 Virtual COM Port)
 *        Pins: PA2 = LPUART1_TX, PA3 = LPUART1_RX (AF12)
 *        Baud Rate: 921600, 8 Data Bits, 1 Stop Bit, No Parity
 */
static void MX_LPUART1_UART_Init(void)
{
    __HAL_RCC_LPUART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PA2 -> LPUART1_TX, PA3 -> LPUART1_RX */
    GPIO_InitStruct.Pin = VCP_TX_PIN | VCP_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_LPUART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    hlpuart1.Instance = LPUART1;
    hlpuart1.Init.BaudRate = 921600;
    hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
    hlpuart1.Init.StopBits = UART_STOPBITS_1;
    hlpuart1.Init.Parity = UART_PARITY_NONE;
    hlpuart1.Init.Mode = UART_MODE_TX_RX;
    hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&hlpuart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief SPI2 Initialization Function (LSM6DS3 IMU)
 *        Pins: PB13 = SCK, PB14 = MISO, PB15 = MOSI (AF5)
 *        Mode: Master, SPI Mode 3 (CPOL=1, CPHA=2Edge), 5.31 MHz (PCLK1 85MHz / 16)
 */
static void MX_SPI2_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PB13 -> SPI2_SCK, PB14 -> SPI2_MISO, PB15 -> SPI2_MOSI */
    GPIO_InitStruct.Pin = IMU_SCK_PIN | IMU_MISO_PIN | IMU_MOSI_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* 85 MHz / 16 = 5.3125 MHz */
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 7;
    hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_SPI3_Init(void)
{
    __HAL_RCC_SPI3_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SD_SCK_PIN | SD_MISO_PIN | SD_MOSI_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    hspi3.Instance = SPI3;
    hspi3.Init.Mode = SPI_MODE_MASTER;
    hspi3.Init.Direction = SPI_DIRECTION_2LINES;
    hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi3.Init.NSS = SPI_NSS_SOFT;
    hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi3.Init.CRCPolynomial = 7;
    hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi3.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi3) != HAL_OK) Error_Handler();
}

/**
 * @brief GPIO Initialization Function
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
 * @brief This function is executed in case of error occurrence.
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        /* Fatal error trap */
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif /* USE_FULL_ASSERT */

/***************************** END OF FILE ************************************/
