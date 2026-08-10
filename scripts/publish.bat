@echo off
chcp 65001 >nul
setlocal
set REPO=https://github.com/skaisayyy/aurora-player.git
cd /d "%~dp0.."

git --version >nul 2>&1
if errorlevel 1 (
  echo [!] Git не найден. Установи: https://git-scm.com/download/win
  pause
  exit /b 1
)

echo == Публикация Aurora Player в %REPO%
if not exist .git git init
git add -A
git commit -m "Aurora Player 1.0.0" 2>nul || echo (нечего коммитить)
git branch -M main
git remote remove origin >nul 2>&1
git remote add origin %REPO%
git push -u origin main
if errorlevel 1 (
  echo [!] Push не удался. Проверь, что репозиторий aurora-player создан и пустой.
  pause
  exit /b 1
)

echo.
echo Готово. Сборка установщика: https://github.com/skaisayyy/aurora-player/actions
echo Через ~10 минут: https://github.com/skaisayyy/aurora-player/releases
pause
