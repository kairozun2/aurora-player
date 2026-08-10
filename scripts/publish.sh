#!/usr/bin/env bash
set -e
REPO="https://github.com/skaisayyy/aurora-player.git"
cd "$(dirname "$0")/.."

command -v git >/dev/null || { echo "[!] Установи git"; exit 1; }

echo "== Публикация Aurora Player в $REPO"
[ -d .git ] || git init
git add -A
git commit -m "Aurora Player 1.0.0" || true
git branch -M main
git remote remove origin 2>/dev/null || true
git remote add origin "$REPO"
git push -u origin main

echo
echo "Готово. Сборка: https://github.com/skaisayyy/aurora-player/actions"
echo "Через ~10 минут: https://github.com/skaisayyy/aurora-player/releases"
