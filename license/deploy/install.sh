#!/usr/bin/env bash
# Установка лицензионного сервера «18:32 cheat» на VPS (Ubuntu/Debian).
#
# Запускать от root из папки license/deploy:
#   bash install.sh
#
# Ожидаемая структура (загружается с твоего ПК, см. README-VPS.md):
#   /opt/1832/license/server/...
#   /opt/1832/license/bot/...
#   /opt/1832/license/keylib.py
#   /opt/1832/license/requirements.txt
#   /opt/1832/license/deploy/install.sh

set -euo pipefail

if [[ $EUID -ne 0 ]]; then
    echo "Запусти от root: sudo bash install.sh" >&2
    exit 1
fi

LICENSE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APP_DIR="$(dirname "$LICENSE_DIR")"

echo "[1/6] Проверяю файлы проекта..."
for item in "server/run.py" "server/main.py" "keylib.py" "requirements.txt"; do
    if [[ ! -f "$LICENSE_DIR/$item" ]]; then
        echo "Не найден: $LICENSE_DIR/$item" >&2
        echo "Загрузи папку license целиком (см. README-VPS.md)." >&2
        exit 1
    fi
done

echo "[2/6] Устанавливаю python3 и venv..."
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq python3 python3-venv python3-pip curl >/dev/null

echo "[3/6] Создаю виртуальное окружение и ставлю зависимости..."
python3 -m venv "$LICENSE_DIR/.venv"
"$LICENSE_DIR/.venv/bin/pip" install --quiet --upgrade pip
"$LICENSE_DIR/.venv/bin/pip" install --quiet -r "$LICENSE_DIR/requirements.txt"

echo "[4/6] Готовлю config.json..."
if [[ ! -f "$LICENSE_DIR/config.json" ]]; then
    cp "$LICENSE_DIR/config.example.json" "$LICENSE_DIR/config.json"
    echo
    echo "  !!! ВНИМАНИЕ: создан config.json из примера."
    echo "  !!! Обязательно поменяй admin_token:"
    echo "      nano $LICENSE_DIR/config.json"
    echo
else
    echo "      config.json уже существует — не трогаю."
fi

echo "[5/6] Устанавливаю systemd-сервис (автозапуск и перезапуск при сбое)..."
UNIT_SRC="$LICENSE_DIR/deploy/1832-license.service"
UNIT_DST="/etc/systemd/system/1832-license.service"
sed "s|__APP_DIR__|$APP_DIR|g" "$UNIT_SRC" > "$UNIT_DST"
systemctl daemon-reload
systemctl enable 1832-license.service >/dev/null 2>&1 || true
systemctl restart 1832-license.service

echo "[6/6] Жду запуск и проверяю health..."
sleep 2
if curl -fsS http://127.0.0.1:8000/api/health >/tmp/1832-health.json 2>/dev/null; then
    echo "      Ответ: $(cat /tmp/1832-health.json)"
else
    echo "      Сервис не ответил, смотрим логи:"
    systemctl status 1832-license.service --no-pager || true
    journalctl -u 1832-license.service -n 30 --no-pager || true
    exit 1
fi

SERVER_IP="$(curl -fsS -4 https://ifconfig.me 2>/dev/null || hostname -I | awk '{print $1}')"
echo
echo "============================================"
echo " Готово. Сервер лицензий работает."
echo " Внешний адрес:  http://$SERVER_IP:8000"
echo " Проверка:        http://$SERVER_IP:8000/api/health"
echo
echo " Убедись, что порт 8000 открыт в панели VPS"
echo " (и в ufw, если он включён: ufw allow 8000/tcp)."
echo " Прописывай этот адрес в DLL или в"
echo " %APPDATA%\\18_32_cheat\\server.txt у покупателей."
echo "============================================"
