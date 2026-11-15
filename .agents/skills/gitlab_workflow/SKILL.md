---
name: gitlab_workflow
description: Полный регламент работы с Git и GitLab (gl.hakaton.1440.space) для агентов: песочница Windows, CRLF, кириллица, URL-encoding и Protected main.
---

# Регламент Git & GitLab Workflow для Team-04

> **Цель:** Исключить повторение ошибок при коммитах, слияниях и отправке кода в репозитории проекта на платформе Windows и в изолированной песочнице агентов.

---

## 1. Архитектура репозиториев

| Remote | URL | Рабочая ветка | Назначение |
|---|---|---|---|
| **`origin`** | `https://github.com/Harimka/hahaton.git` | `developV2` | Основной командный репозиторий (разработка, задачи) |
| **`gitlab`** | `https://gl.hakaton.1440.space/hackathon-2026/stage-1/team-04.git` | `main` / `developv2` | Официальный репозиторий хакатона для проверки жюри |

---

## 2. Почему возникали задержки и ошибки (Разбор граблей)

При первой попытке коммитов возникли 5 независимых барьеров, которые привели к серии повторных попыток:

### 1. Изоляция песочницы (Sandbox Isolation)
* **Проблема:** По умолчанию терминал агента запускается в изолированном режиме (`BypassSandbox: false`). В этом режиме **полностью заблокирован сетевой стек** (ошибка `getaddrinfo() thread failed to start`) и запрещена запись в служебный каталог `.git` (`Permission denied: could not lock config file`).
* **Решение:** Все команды Git, обращающиеся к сети (`fetch`, `push`) или модифицирующие репозиторий (`commit`, `checkout`, `merge`, `remote add`), **обязательно** запускаются с `BypassSandbox: true`.

### 2. Защита ветки `main` в GitLab (Protected Branch)
* **Проблема:** Серверный хук GitLab отклонил прямой пуш:
  ```text
  remote: GitLab: You are not allowed to push code to protected branches on this project.
  ! [remote rejected] main -> main (pre-receive hook declined)
  ```
* **Решение:** Разработчики не имеют права напрямую перезаписывать `main`. В `main` код доставляется **только через Merge Request**!

### 3. Символ `@` в логине (Email в URL)
* **Проблема:** Логин выдан в виде email (`stage1-team4-5@example.invalid`). Если подставить его в URL напрямую, Git и HTTP-клиенты парсят часть после `@` как имя хоста.
* **Решение:** Символ `@` в логине **всегда экранируется как `%40`**:
  ```text
  https://stage1-team4-5%40example.invalid:PASSWORD@gl.hakaton.1440.space/...
  ```

### 4. Несвязанные истории коммитов (Unrelated Histories)
* **Проблема:** Репозиторий на GitLab был инициализирован отдельно от локального (в него через браузер загрузили файлы схемотехники `СХЕМОТЕХНИКА/` и pdf). У локальной ветки и ветки `gitlab/main` не было общего корня (`merge-base` возвращал ошибку).
* **Решение:** Слияние выполняется с флагом `--allow-unrelated-histories`:
  ```bash
  git merge developV2 --allow-unrelated-histories
  ```

### 5. Алиасы PowerShell и переводы строк CRLF
* **Проблема:** В PowerShell команда `curl` является псевдонимом `Invoke-WebRequest`, а Windows Git по умолчанию ругается на CRLF в файлах ST HAL.
* **Решение:** Вызывать бинарник напрямую (`curl.exe`), а в git настроить `core.autocrlf true` и `core.quotepath false`.

---

## 3. Пошаговый стандарт (SOP) отправки изменений

### Шаг 1: Фиксация изменений в рабочей ветке
```powershell
# 1. Проверяем статус
git status --short

# 2. Добавляем измененные файлы (без build/ и временных логов!)
git add <файлы>

# 3. Делаем коммит по Conventional Commits (на английском)
git commit -m "feat(module): description of changes"
```

### Шаг 2: Отправка в командный GitHub
```powershell
git push origin developV2
```

### Шаг 3: Отправка в ветку разработки GitLab
```powershell
git push gitlab developV2:developv2
```

### Шаг 4: Доставка в защищенный `main` через автоматический Merge Request
Чтобы обновить судейский `main`, отправляем ветку с push-опцией создания MR:
```powershell
# Вариант А: Автоматическое создание MR через Git Push Options
git push -o merge_request.create -o merge_request.target=main -o merge_request.title="feat: Integrate developv2 into main" gitlab developV2:integrate-main

# Вариант Б: Принятие MR
# Перейдите по ссылке, выведенной GitLab в терминал, и нажмите кнопку 'Merge'.
```

---

## 4. Чек-лист перед коммитом
- [ ] Параметр команды установлен в `BypassSandbox: true` (для сетевых и пишущих git-команд).
- [ ] В индекс не попали файлы из `firmware/build/`, `*.log`, `*.elf`, `*.hex`.
- [ ] Проект компилируется локально (`build.bat` возвращает код 0).
- [ ] Сохранены файлы схемотехники и документация других участников.
