"""HTTP API лицензионного сервера «18:32 cheat».

Публичные методы (использует loader):
- POST /api/activate — активировать ключ; первая активация запускает срок.
- POST /api/status   — проверить ключ без активации (остаток дней).

Админские методы (Bearer admin_token, использует Telegram-бот):
- POST /api/admin/keys           — создать ключи на N дней.
- GET  /api/admin/keys           — список ключей.
- POST /api/admin/keys/<key>/revoke    — отозвать ключ.
- POST /api/admin/keys/<key>/restore   — вернуть отозванный ключ.
- POST /api/admin/keys/<key>/reset-hwid — отвязать ключ от ПК.
"""

from __future__ import annotations

from typing import Optional

from fastapi import FastAPI, Header, HTTPException
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field

from .config import bind_hwid_enabled, server_settings
from .db import LicenseStore


class LicenseRequest(BaseModel):
    key: str = Field(min_length=4, max_length=64)
    hwid: Optional[str] = Field(default=None, max_length=128)


class AdminCreateRequest(BaseModel):
    days: int = Field(ge=1, le=3650)
    count: int = Field(default=1, ge=1, le=50)
    note: str = Field(default="", max_length=200)


def create_app() -> FastAPI:
    settings = server_settings()
    app = FastAPI(title="18:32 cheat license server", version="1.0.0")
    app.state.store = LicenseStore(settings.database_path)
    app.state.admin_token = settings.admin_token

    def require_admin(authorization: Optional[str]) -> None:
        if not authorization or not authorization.startswith("Bearer "):
            raise HTTPException(status_code=401, detail="unauthorized")
        if authorization[len("Bearer "):].strip() != app.state.admin_token:
            raise HTTPException(status_code=401, detail="unauthorized")

    @app.post("/api/activate")
    def activate(request: LicenseRequest) -> JSONResponse:
        code, payload = app.state.store.activate(
            request.key, request.hwid, bind_hwid_enabled()
        )
        return JSONResponse(status_code=code, content=payload)

    @app.post("/api/status")
    def status(request: LicenseRequest) -> JSONResponse:
        code, payload = app.state.store.status(
            request.key, request.hwid, bind_hwid_enabled()
        )
        return JSONResponse(status_code=code, content=payload)

    @app.get("/api/health")
    def health() -> dict:
        return {"status": "ok", "keys": len(app.state.store.list_keys())}

    @app.post("/api/admin/keys")
    def create_keys(request: AdminCreateRequest, authorization: Optional[str] = Header(default=None)) -> dict:
        require_admin(authorization)
        keys = app.state.store.create_keys(request.days, request.count, request.note.strip())
        return {"status": "ok", "keys": keys}

    @app.get("/api/admin/keys")
    def list_keys(authorization: Optional[str] = Header(default=None)) -> dict:
        require_admin(authorization)
        return {"status": "ok", "keys": app.state.store.list_keys()}

    def admin_key_action(key: str, action) -> dict:
        record = action(key)
        if record is None:
            raise HTTPException(status_code=404, detail="invalid")
        return {"status": "ok", "key": record}

    @app.post("/api/admin/keys/{key}/revoke")
    def revoke(key: str, authorization: Optional[str] = Header(default=None)) -> dict:
        require_admin(authorization)
        return admin_key_action(key, lambda value: app.state.store.set_revoked(value, True))

    @app.post("/api/admin/keys/{key}/restore")
    def restore(key: str, authorization: Optional[str] = Header(default=None)) -> dict:
        require_admin(authorization)
        return admin_key_action(key, lambda value: app.state.store.set_revoked(value, False))

    @app.post("/api/admin/keys/{key}/reset-hwid")
    def reset_hwid(key: str, authorization: Optional[str] = Header(default=None)) -> dict:
        require_admin(authorization)
        return admin_key_action(key, app.state.store.reset_hwid)

    return app


app = create_app()
