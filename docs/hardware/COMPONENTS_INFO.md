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

## 5. Уточнения для FIFO, DMA и SPI

Источник для LSM6DS3: `docs/datasheets/lsm6ds3_reg.h` и [даташит ST DocID026899 Rev 10](https://community.st.com/ysqtg83639/attachments/ysqtg83639/stm32-mcu-products-forum/89677/2/%5BATT%5D11987.pdf); для процедуры FIFO — [ST AN4650](https://community.st.com/ysqtg83639/attachments/ysqtg83639/mems-sensors-forum/21052/2/AN4650.PDF).

* SPI LSM6DS3: режим 3 или 0, не выше 10 МГц; выбраны mode 3, **8-битный кадр**, MSB first. Команда чтения — адрес с битом 7 = 1, далее данные по байтам при удержании CS. FIFO burst из N XYZ занимает `1 + 6*N` байт; принятый во время команды байт не является данными.
* `CTRL4_C` (`0x13`), `XL_BW_SCAL_ODR` бит 7 = 0: при 6,66 кГц аналоговый anti-alias фильтр обходится, а цифровой LPF1 имеет номинальную частоту среза 3320 Гц (AN4650, таблицы 7–8). Принудительный режим 400 Гц не выбран, так как он дополнительно подавляет верхнюю часть SRS. Отсутствие аналогового anti-alias фильтра и реальная АЧХ датчика требуют стендовой оценки; ODR не гарантирует точность на 2113 Гц. После включения датчика ST рекомендует отбросить четыре первых отсчёта; пример ждёт 2 мс до включения FIFO.
* `CTRL6_C` (`0x15`), `XL_HM_MODE` бит 4 = 0: high-performance. `CTRL8_XL` (`0x17`), LPF2 и HP/slope выключены. `CTRL9_XL` (`0x18`), биты XYZ 5:3 = 111.
* `FIFO_CTRL1/2` (`0x06/0x07`): watermark — 12-битное число **16-битных слов**; 16 XYZ = 48 слов. `FIFO_CTRL3` (`0x08`) = `0x01`: акселерометр без децимации, гироскоп вне FIFO. `FIFO_CTRL4` (`0x09`) = 0: прочие наборы выключены. `FIFO_CTRL5` (`0x0A`) = `0x56`: FIFO ODR 6,66 кГц и continuous mode. `INT1_CTRL` (`0x0D`) бит 3 = watermark.
* `FIFO_STATUS1..4` (`0x3A..0x3D`): количество непрочитанных слов, empty/full/overrun/watermark и индекс следующего слова pattern. `FIFO_STATUS2`: empty `0x10`, full `0x20`, overrun `0x40`, watermark `0x80`; `FIFO_DATA_OUT_L/H` — `0x3E/0x3F`. При многобайтовом чтении FIFO-порт циклически возвращает L/H. При полном FIFO в continuous mode старые данные перезаписываются.
* Принятый выбор проекта для NUCLEO-G474RE: SPI2 — PB13/14/15 AF5, SPI3 — PC10/11/12 AF6. HCLK 170 МГц, APB1 `/2` = 85 МГц: SPI2 `/16` ≈ 5,3125 МГц, SPI3 `/256` ≈ 332 кГц при инициализации SD и `/16` ≈ 5,3125 МГц после неё. SPI3 работает от PCLK1; удвоенная частота таймеров при APB1 `/2` к SPI3 не относится. APB1 делится на 2, поэтому TIM2/TIM6 получают 170 МГц; делитель 170 даёт счётчик 1 МГц. Источники: [даташит STM32G474](https://www.st.com/resource/en/datasheet/stm32g474rb.pdf), [RM0440](https://www.st.com/resource/en/reference_manual/dm00355726-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).
* Примечание SPI adapter: для SPI1 частота берётся от PCLK2, для SPI2/SPI3 — от PCLK1. На этой конфигурации APB1=85 МГц, так что запрос SD 10 МГц корректно округляется вниз к ближайшей доступной частоте 5,3125 МГц (`/16`), а не отклоняется.
* Вариант распределения ресурсов для интеграции: SPI2 RX — DMA1 Channel 1 / DMAMUX `DMA_REQUEST_SPI2_RX`, TX — DMA1 Channel 2 / `DMA_REQUEST_SPI2_TX`; память буферов — SRAM, доступная DMA. Каналы являются **резервированием проекта**, их нужно проверить на конфликт с каркасом Сени. [HAL G4 DMA](https://github.com/STMicroelectronics/stm32g4xx-hal-driver/blob/master/Inc/stm32g4xx_hal_dma.h).
* Частота 6,66 кГц относится к судейскому LSM6DS3; требование ≥10 кГц относится к выбору акселерометра для новой платы, не к этому датчику.
