---
name: gitlab_workflow
description: Стандарт и регламент синхронизации кода, коммитов и Merge Request в GitLab репозиторий проекта (gl.hakaton.1440.space).
---

# Регламент работы с GitLab репозиторием (GitLab Workflow Protocol)

Данный документ описывает правила и точную последовательность шагов для отправки коммитов и веток в GitLab (`gl.hakaton.1440.space`).

---

## 1. Ключевые особенности и ограничения окружения

### 1.1. URL-Encoding логина (Символ `@` в email)
* При передаче логина в формате email (`stage1-team4-5@example.invalid`) в URL репозитория символ `@` **обязательно** экранируется как `%40`:
  ```bash
  # ПРАВИЛЬНО:
  https://stage1-team4-5%40example.invalid:PASSWORD@gl.hakaton.1440.space/hackathon-2026/stage-1/team-04.git

  # ОШИБКА (git воспримет часть email как хост):
  https://stage1-team4-5@example.invalid:PASSWORD@gl.hakaton.1440.space/...
  ```

### 1.2. Доступ к сети и каталогу `.git` (Sandbox Bypass)
* В изолированном окружении терминала запись в каталог `.git` и обращение по HTTPS блокируются политикой изоляции.
* Все команды, изменяющие состояние `.git` (`git commit`, `git checkout`, `git merge`, `git remote add`) или обращающиеся к сети (`git push`, `git fetch`), должны выполняться с отключенной изоляцией (`BypassSandbox: true`).

### 1.3. Защита ветки `main` (Protected Branch)
* В репозитории GitLab ветка `main` является защищенной (`Protected`).
* Прямой пуш (`git push gitlab main`) **запрещен** серверным хуком:
  `remote: GitLab: You are not allowed to push code to protected branches on this project.`
* Все изменения в `main` должны поступать **исключительно через Merge Request**!

### 1.4. Несвязанные истории коммитов (Unrelated Histories)
* Репозиторий на GitLab и локальный репозиторий могут иметь независимые корневые коммиты (например, в GitLab через веб-интерфейс была загружена папка `СХЕМОТЕХНИКА`, а локально создана структура прошивки).
* При слиянии обязательно указывать флаг:
  ```bash
  git merge <branch> --allow-unrelated-histories -m "..."
  ```

---

## 2. Стандартный алгоритм внесения изменений и доставки в `main`

### Шаг 1. Проверка и фиксация изменений в рабочей ветке (`developV2`)
```bash
git status
git add <files>
git commit -m "feat: <Краткое описание на английском>"
```

### Шаг 2. Заливка ветки в GitLab
```bash
# Пуш в ветку на GitLab
git push gitlab developV2:developv2
```

### Шаг 3. Интеграция с GitLab `main` (если требуется обновление `main`)
Поскольку ветка `main` защищена от прямой записи:
1. Забираем актуальное состояние `main` из GitLab:
   ```bash
   git fetch gitlab
   ```
2. Создаем временную интеграционную ветку от `gitlab/main`:
   ```bash
   git checkout -B integrate-to-main gitlab/main
   ```
3. Выполняем чистое слияние с рабочей веткой (с сохранением схемотехники и документации):
   ```bash
   git merge developV2 --allow-unrelated-histories -m "feat: Integrate <module> into main"
   ```
4. Отправляем интеграционную ветку с автоматическим созданием Merge Request через Git Push Options:
   ```bash
   git push -o merge_request.create -o merge_request.target=main -o merge_request.title="feat: Integrate changes into main" gitlab integrate-to-main
   ```

### Шаг 4. Принятие Merge Request
* В консоли пуша GitLab возвращает ссылку на созданный MR вида:
  `https://gl.hakaton.1440.space/hackathon-2026/stage-1/team-04/-/merge_requests/<ID>`
* Разработчик или агент подтверждает слияние (нажатием кнопки Merge в веб-интерфейсе либо через API).
* После слияния локальная ветка `main` синхронизируется:
  ```bash
  git checkout main
  git pull --ff-only gitlab main
  ```
* Временная ветка в GitLab удаляется:
  ```bash
  git push gitlab --delete integrate-to-main
  ```

---

## 3. Чек-лист перед коммитом
- [ ] Все временные файлы логов (`*.log`, `build/`) добавлены в `.gitignore`.
- [ ] Исходный код прошивки компилируется без ошибок (`build.bat`).
- [ ] Соблюдены правила Doxygen-шапки (`@file`, `@author team-04`) и C++17 архитектуры.
- [ ] Файлы схемотехники и документации других участников команды не удалены.
