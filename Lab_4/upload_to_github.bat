@echo off
chcp 65001 >nul
REM Скрипт для загрузки проекта в GitHub

echo ========================================
echo Загрузка проекта в GitHub
echo ========================================
echo.

REM Проверка наличия git
where git >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Git не найден!
    echo Пожалуйста, установите Git: https://git-scm.com/download/win
    pause
    exit /b 1
)

echo Git найден. Начинаю загрузку...
echo.

REM Настройка git для правильной работы с UTF-8
git config --global core.quotepath false
git config --global i18n.commitencoding utf-8
git config --global i18n.logoutputencoding utf-8
echo.

REM Инициализация репозитория (если еще не инициализирован)
if not exist ".git" (
    echo Инициализация git репозитория...
    git init
    echo.
)

REM Добавление всех файлов
echo Добавление файлов...
git add .
echo.

REM Проверка, есть ли изменения для commit
git diff --cached --quiet
if %ERRORLEVEL% EQU 0 (
    echo Нет изменений для commit. Проверяю существующие коммиты...
    git log --oneline -1 >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo Создание первого commit...
        git commit -m "Добавил лабу 4"
    ) else (
        echo Уже есть коммиты. Пропускаю создание commit.
    )
) else (
    echo Создание commit...
    git commit -m "Добавил лабу 4"
)
echo.

REM Добавление remote (если еще не добавлен)
git remote get-url origin >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Добавление remote репозитория...
    git remote add origin https://github.com/Voronyanskiy-Danila/cgwork4.git
    echo.
) else (
    echo Remote уже настроен.
    echo.
)

REM Переименование ветки в main (если нужно)
git branch --show-current >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    for /f "tokens=*" %%i in ('git branch --show-current') do set CURRENT_BRANCH=%%i
    if not "!CURRENT_BRANCH!"=="main" (
        echo Переименование ветки в main...
        git branch -M main
        echo.
    )
)

REM Загрузка в GitHub
echo Загрузка в GitHub...
echo ВНИМАНИЕ: Вам может потребоваться ввести логин и пароль (или токен доступа)
echo.
git push -u origin main

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Успешно загружено в GitHub!
    echo ========================================
    echo Репозиторий: https://github.com/Voronyanskiy-Danila/cgwork4
    echo.
) else (
    echo.
    echo ========================================
    echo ОШИБКА при загрузке!
    echo ========================================
    echo Возможные причины:
    echo 1. Неверные учетные данные
    echo 2. Репозиторий не существует или нет доступа
    echo 3. Нужно настроить аутентификацию
    echo.
    echo Для настройки аутентификации используйте:
    echo   git config --global user.name "Ваше имя"
    echo   git config --global user.email "ваш@email.com"
    echo.
    echo Или используйте Personal Access Token вместо пароля
    echo.
)

pause

