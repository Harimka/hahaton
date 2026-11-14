# AGENTS.md — Team-04 AI Harness

> Компактный паспорт проекта. Агенты обязаны следовать этим правилам при генерации любого кода.

---

## 1. Контекст, архитектурный стандарт и роли
* **Стандарт индустрии:** C++17 Embedded (паттерн слоеных интерфейсов: Hardware Abstraction Layer, чисто виртуальные интерфейсы C++). См. подробности в [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).
* **Аппаратная платформа:** См. [`docs/hardware/COMPONENTS_INFO.md`](docs/hardware/COMPONENTS_INFO.md) (STM32G474RE @ 170 МГц, LSM6DS3 по SPI2 6.66 кГц, SD по SPI3, LED PA5).
* **Среда разработки:** STM32CubeMX + STM32CubeIDE (или расширение STM32 для VS Code).
* **Зоны ответственности (не менять чужие модули!):**
  1. `firmware/App/` — FSM суперцикла, границы импульса 0.25*Amax, буферы.
  2. `firmware/Drivers/bsp/` — Драйвер акселерометра (наследует `ImuDrv` через `SpiDrv`).
  3. `firmware/App/` — Бортовой расчет SRS (FPU, Ramp Invariant, Maximax).
  4. `firmware/App/` — SD-кардридер и запись `raw_signal_XXXX.bin` / `raw_srs_XXXX.bin`.
  5. `scripts/` & `hardware/` — Скрипт ПК и схемотехника (12V USB-C + 72ч АКБ).

---

## 2. Hard Constraints (Жесткие запреты)
1. **Запрет динамической памяти:** `malloc`, `free`, `new`, `delete`, `std::vector` запрещены. Вся память выделяется статически на этапе компиляции.
2. **Запрет блокировок:** `HAL_Delay()` и бесконечные циклы `while(1)` запрещены везде, кроме Fatal Error Handler.
3. **Изоляция слоев (C++ Интерфейсы):**
   - Драйвер внешнего чипа (`Lsm6ds3`) наследует `ImuDrv` и принимает ссылку/указатель на `SpiDrv`.
   - Драйвер чипа не знает про STM32 HAL! Работа с шиной идет ТОЛЬКО через методы `SpiDrv`.
   - Конкретная реализация `Stm32G4Spi` живет в слое Port/BSP.
4. **Стандарты C++:** `enum class` со значениями `kName`, поля `camelCase_`, конструкторы с Leading Commas, запрет копирования (`= delete`).

---

## 3. Регламент работы с даташитами (Grounding Rule)
* **Запрет додумывания регистров:** Агентам запрещено брать адреса регистров или чувствительность из головы.
* **Порядок поиска (Skill `doc_retrieval`):**
  1. Читать выгруженную информацию в [`docs/hardware/COMPONENTS_INFO.md`](docs/hardware/COMPONENTS_INFO.md).
  2. Если параметров не хватает — найти их в `docs/datasheets/` (`lsm6ds3_reg.h`) и **ОБЯЗАТЕЛЬНО записать в `COMPONENTS_INFO.md`**, чтобы другие агенты не перечитывали даташиты повторно.

---

## 4. Оформление кода
* Язык кода и комментариев: **строго английский**.
* Обязательна стандартная Doxygen-шапка (`@file`, `@author team-04`, `@brief`) и футер `END OF FILE`.

---

## 5. Регламент коммитов и GitLab (Skill `gitlab_workflow`)
* Логин с email экранируется: `@` -> `%40` в HTTPS URL.
* Ветка `main` в GitLab **защищена**: прямой пуш запрещен серверным хуком.
* Доставка изменений в `main` выполняется через слияние (`--allow-unrelated-histories`) и Merge Request (через `git push -o merge_request.create ...`). Подробный чек-лист: [`.agents/skills/gitlab_workflow/SKILL.md`](.agents/skills/gitlab_workflow/SKILL.md).

