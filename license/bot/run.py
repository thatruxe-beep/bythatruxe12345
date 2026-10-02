"""Запуск Telegram-бота: python -m license.bot.run"""

from ..server.config import telegram_settings
from .api_client import LicenseApiClient
from .handlers import BotHandlers
from .telegram import TelegramTransport


def main() -> None:
    settings = telegram_settings()
    if not settings.bot_token:
        raise SystemExit(
            "Не задан токен бота. Получите его у @BotFather и впишите"
            " в license/config.json (telegram.bot_token)."
        )
    if not settings.admin_ids:
        raise SystemExit(
            "Не задан администратор. Впишите свой числовой id"
            " в license/config.json (telegram.admin_ids)."
        )

    api = LicenseApiClient()
    print(f"[bot] сервер лицензий: {api.base_url}")
    transport = TelegramTransport(settings.bot_token, BotHandlers(api, settings.admin_ids))
    try:
        transport.run()
    except KeyboardInterrupt:
        print("\n[bot] остановлен")
    finally:
        transport.close()


if __name__ == "__main__":
    main()
