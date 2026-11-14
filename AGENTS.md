# AGENTS.md — Team-04 AI Harness

> Компактный паспорт проекта. Агенты обязаны следовать этим правилам при генерации любого кода.

---

## 1. Контекст, железо и роли (5 человек)
* **Аппаратная платформа:** См. [`docs/hardware/COMPONENTS_INFO.md`](docs/hardware/COMPONENTS_INFO.md) (STM32G474RE @ 170 МГц, LSM6DS3 по SPI2 6.66 кГц, SD по SPI3, LED PA5).
* **Среда разработки:** STM32CubeMX + STM32CubeIDE (или расширение STM32 для VS Code).
* **Зоны ответственности (не менять чужие модули!):**
  1. `firmware/App/Src/trigger_fsm.c` — FSM суперцикла и границы импульса.
  2. `firmware/Drivers/bsp/Src/lsm6ds3.c` — Драйвер акселерометра (SPI).
  3. `firmware/App/Src/srs_smallwood.c` — Бортовой расчет SRS (FPU, Ramp Invariant).
  4. `firmware/App/Src/sd_logger.c` — Запись raw_signal_XXXX.bin и raw_srs_XXXX.bin.
  5. `scripts/srs_analyzer.py` & `hardware/` — Скрипт ПК и схемотехника (12V USB-C + 72ч АКБ).

---

## 2. Hard Constraints (Жесткие запреты)
1. **Запрет блокировок:** `HAL_Delay()` и бесконечные циклы `while(1)` запрещены везде, кроме Fatal Error Handler.
2. **Запрет динамической памяти:** `malloc`, `free`, `new` запрещены. Вся память выделяется статически.
3. **Изоляция слоев:**
   - **App Layer:** Не знает про STM32 и HAL. Работает только через интерфейсы в `Drivers/bsp/Inc/`.
   - **Driver Layer:** Знает только регистры своего внешнего чипа (в `.h`). Все операции с шиной — строго через абстрактный `bus_spi.h`.
   - **Port/BSP Layer:** Единственное место, где разрешен HAL/LL конкретного МК.
4. **Типы данных:** Только фиксированная ширина из `<stdint.h>` (`uint8_t`, `int16_t`, `uint32_t`, `float`).

---

## 3. Регламент работы с даташитами и документацией (Grounding Rule)
* **Запрет додумывания регистров:** Агентам запрещено брать адреса регистров или чувствительность из головы.
* **Порядок поиска (Skill `doc_retrieval`):**
  1. Читать выгруженную информацию в [`docs/hardware/COMPONENTS_INFO.md`](docs/hardware/COMPONENTS_INFO.md).
  2. Если параметров не хватает — найти их в `docs/datasheets/` или официальной документации и **ОБЯЗАТЕЛЬНО записать в `COMPONENTS_INFO.md`**, чтобы другие агенты не перечитывали даташиты повторно.

---

## 4. Оформление кода
* Язык кода и комментариев: **строго английский**.
* Обязательна стандартная Doxygen-шапка (`@file`, `@author team-04`, `@brief`) и футер `END OF FILE`.
* Архитектура системы: см. [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).
