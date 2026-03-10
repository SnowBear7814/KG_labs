@echo off
chcp 65001 >nul
REM Скрипт для коммита и загрузки изменений в GitHub

echo ========================================
echo Коммит и загрузка в GitHub
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

echo Git найден.
echo.

REM Настройка git для правильной работы с UTF-8
git config --global core.quotepath false
git config --global i18n.commitencoding utf-8
git config --global i18n.logoutputencoding utf-8

REM Добавление всех изменений
echo Добавление изменений...
git add .
echo.

REM Создание коммита
echo Создание коммита с сообщением "Сделал доп"...
git commit -m "Сделал доп"
echo.

REM Проверка наличия remote
git remote get-url origin >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Добавление remote репозитория...
    git remote add origin https://github.com/Voronyanskiy-Danila/cgwork4.git
    echo.
)

REM Загрузка в GitHub
echo Загрузка в GitHub...
echo ВНИМАНИЕ: Вам может потребоваться ввести логин и пароль (или токен доступа)
echo.
git push origin main

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
    echo Попробуйте выполнить вручную:
    echo   git push origin main
    echo.
)

pause

