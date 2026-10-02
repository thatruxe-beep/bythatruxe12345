"""Запуск лицензионного сервера: python -m license.server.run"""

import uvicorn

from .config import server_settings
from .main import create_app


def main() -> None:
    settings = server_settings()
    print(f"[license] база данных: {settings.database_path}")
    print(f"[license] сервер: http://{settings.host}:{settings.port}")
    uvicorn.run(create_app(), host=settings.host, port=settings.port)


if __name__ == "__main__":
    main()
