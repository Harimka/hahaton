# Hardware Specifications & Verified Component Data

> **ЭТОТ ФАЙЛ — ЕДИНЫЙ ИСТОЧНИК ПРАВДЫ ДЛЯ АГЕНТОВ (Grounding Cache).**
> При реализации драйверов и схемотехники агенты обязаны брать адреса регистров, пины, чувствительность и частоты строго отсюда.
> Если вы извлекаете новую информацию из даташитов — ДОПИСЫВАЙТЕ ЕЁ СЮДА.

---

## 1. Спецификация оборудования стенда

| Компонент | Модель / Описание | Интерфейс | Примечания |
|---|---|---|---|
| **MCU** | STM32G474RE (Cortex-M4F @ 170 МГц, FPU, 512 KB Flash, 128 KB SRAM) | Внутренний | Внутренняя частота 170 МГц (CubeMX) |
| **Плата отладочная** | Nucleo-G474RE | ST-LINK V3 / USB VCP | Встроенный LED2 (PA5) |
| **Акселерометр** | LSM6DS3 (3-осевой акселерометр + гироскоп) | SPI (SPI2) | 6.66 кГц ODR, full-scale ±16g |
| **SD-модуль** | Waveshare MicroSD Board | SPI (SPI3) | Питание строго 3.3 В (без встроенного LDO) |
| **SD-карта** | Transcend 300S UHS-I 64 ГБ | SPI / FatFS / Raw | Форматирование FAT32 / Raw блоки |

---

## 2. Распиновка подключения (Strict Pinout Matrix)

### Акселерометр LSM6DS3 $\leftrightarrow$ STM32G474RE
* **SPI SCK:** `PB13` (SPI2_SCK)
* **SPI MISO (SDO):** `PB14` (SPI2_MISO)
* **SPI MOSI (SDI):** `PB15` (SPI2_MOSI)
* **Chip Select (CS):** `PB12` (GPIO Output, Active LOW)
* **Interrupt 1 (INT1):** `PC4` (GPIO EXTI4, Falling/Rising edge от DRDY)

### Модуль MicroSD $\leftrightarrow$ STM32G474RE
* **SPI SCK / CLK:** `PC10` (SPI3_SCK)
* **SPI MISO / DO:** `PC11` (SPI3_MISO)
* **SPI MOSI / DI:** `PC12` (SPI3_MOSI)
* **Chip Select (CS):** `PA4` (GPIO Output, Active LOW)
* **Питание (VCC/GND):** **Строго 3.3V** от платы Nucleo к модулю Waveshare.

### Пользовательская индикация
* **LD2 (Встроенный светодиод):** `PA5` (GPIO Output, мигание с частотой 0.5 с при превышении порога SRS).

---

## 3. Выверенные параметры LSM6DS3 (Official ST Register Map)

* **WHO_AM_I Регистр:** `0x0F` (Ожидаемое значение: `0x69`).
* **CTRL1_XL (Настройка акселерометра):** `0x10`
  * ODR 6.66 кГц = биты `[7:4] = 1010` (`0xA0`)
  * Full-Scale ±16g = биты `[3:2] = 01` (`0x04`)
  * Значение конфигурации: `0xA4`
* **CTRL3_C (Управление режимами шины):** `0x12`
  * `BDU` (Block Data Update) = бит 6 (`0x40`) — предотвращает чтение неконсистентных байт LSB/MSB
  * `IF_INC` (Автоинкремент адреса при чтении) = бит 2 (`0x04`)
* **STATUS_REG:** `0x1E`
  * `XLDA` (Акселерометр Data Ready) = бит 0 (`0x01`)
* **Регистры выходных данных акселерометра (6 байт, little-endian):**
  * `OUTX_L_XL`: `0x28`, `OUTX_H_XL`: `0x29`
  * `OUTY_L_XL`: `0x2A`, `OUTY_H_XL`: `0x2B`
  * `OUTZ_L_XL`: `0x2C`, `OUTZ_H_XL`: `0x2D`
* **Чувствительность (Sensitivity) для ±16g:**
  * $0.488 \text{ mg/LSB} = 0.000488 \text{ g/LSB}$
  * Формула: $a [g] = \text{raw\_int16} \times 0.000488\text{f}$.

---

## 4. Требования STM32CubeMX / CubeIDE
* **Тактирование:** 170 МГц SYSCLK (PLL от HSI 16 МГц: HSI -> PLLM /4 -> PLLN *85 -> PLLR /2 = 170 МГц).
* **FPU:** Включен аппаратный Single Precision FPU (`-mfpu=fpv4-sp-d16 -mfloat-abi=hard`).
* **SPI2 (LSM6DS3):** Прескалер для тактовой частоты SPI $\le 10$ МГц (LSM6DS3 держит до 10 МГц по SPI).
* **SPI3 (SD Card):** Стартовая инициализация 400 кГц, рабочая скорость 10–20 МГц.
