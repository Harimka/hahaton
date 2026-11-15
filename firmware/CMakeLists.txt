cmake_minimum_required(VERSION 3.20)

project(team04_firmware C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Architecture and Hardware FPU flags for Cortex-M4
set(ARM_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARM_FLAGS} -O3 -Wall -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARM_FLAGS} -O3 -Wall -fdata-sections -ffunction-sections -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${ARM_FLAGS} -x assembler-with-cpp")

# Linker flags
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/STM32G474RETX_FLASH.ld")
set(CMAKE_EXE_LINKER_FLAGS "${ARM_FLAGS} -T${LINKER_SCRIPT} -specs=nano.specs -specs=nosys.specs -u _printf_float -Wl,-Map=${PROJECT_NAME}.map,--cref -Wl,--gc-sections -lc -lm")

# Definitions
add_definitions(
    -DSTM32G474xx
    -DUSE_HAL_DRIVER
)

# Include Directories
include_directories(
    Core/Inc
    App/Inc
    Drivers/bsp/Inc
    Drivers/port/Inc
    Middlewares/Third_Party/FatFs/src
    FATFS/Target
    Drivers/STM32G4xx_HAL_Driver/Inc
    Drivers/STM32G4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32G4xx/Include
    Drivers/CMSIS/Include
)

# HAL Source files needed for application
set(HAL_SOURCES
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_spi.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_spi_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim_ex.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart.c
    Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart_ex.c
)

set(SOURCES
    Core/Src/main.cpp
    Core/Src/stm32g4xx_it.c
    Core/Src/system_stm32g4xx.c
    App/Src/sample_ring_buffer.cpp
    Drivers/bsp/Src/lsm6ds3.cpp
    Drivers/port/Src/stm32g4spi.cpp
    Drivers/port/Src/stm32g4uart.cpp
    Drivers/port/Src/fatfsadapter.cpp
    Drivers/bsp/Src/sdcard.cpp
    Drivers/bsp/Src/sdcard_c.c
    Middlewares/Third_Party/FatFs/src/ff.c
    Middlewares/Third_Party/FatFs/src/diskio.c
    Middlewares/Third_Party/FatFs/src/ff_gen_drv.c
    Middlewares/Third_Party/FatFs/src/option/syscall.c
    Drivers/CMSIS/Device/ST/STM32G4xx/Source/Templates/gcc/startup_stm32g474xx.s
    ${HAL_SOURCES}
)

add_executable(${PROJECT_NAME}.elf ${SOURCES})

# Target post-build commands: generate .hex, .bin, and print size
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND arm-none-eabi-objcopy -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND arm-none-eabi-size ${PROJECT_NAME}.elf
    COMMENT "Building ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin..."
)
