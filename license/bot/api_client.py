"""HTTP-клиент бота к локальному лицензионному серверу."""

from __future__ import annotations

from typing import Any

import httpx

from ..server.config import server_base_url, server_settings


class LicenseApiError(RuntimeError):
    """Сервер лицензий недоступен или отклонил запрос."""


class LicenseApiClient:
    def __init__(
        self,
        base_url: str | None = None,
        admin_token: str | None = None,
        timeout: float = 10.0,
    ) -> None:
        settings = server_settings()
        self.base_url = (base_url or server_base_url()).rstrip("/")
        self.admin_token = admin_token or settings.admin_token
        self.timeout = timeout

    def _request(self, method: str, path: str, json: dict | None = None) -> Any:
        try:
            with httpx.Client(timeout=self.timeout) as client:
                response = client.request(
                    method,
                    self.base_url + path,
                    json=json,
                    headers={"Authorization": f"Bearer {self.admin_token}"},
                )
        except httpx.HTTPError as error:
            raise LicenseApiError(
                f"Сервер лицензий недоступен ({error.__class__.__name__})."
                " Запущен ли python -m license.server.run?"
            ) from error
        if response.status_code == 401:
            raise LicenseApiError("Сервер отклонил admin_token — проверьте config.json.")
        if response.status_code >= 400:
            raise LicenseApiError(f"Сервер вернул ошибку {response.status_code}.")
        return response.json()

    def health(self) -> dict:
        return self._request("GET", "/api/health")

    def create_keys(self, days: int, count: int = 1, note: str = "") -> dict:
        return self._request(
            "POST", "/api/admin/keys", {"days": days, "count": count, "note": note}
        )

    def list_keys(self) -> list[dict]:
        return self._request("GET", "/api/admin/keys").get("keys", [])

    def key_info(self, key: str) -> dict | None:
        target = key.strip().upper()
        for record in self.list_keys():
            if record["key"] == target:
                return record
        return None

    def revoke(self, key: str) -> dict:
        return self._request("POST", f"/api/admin/keys/{key.strip().upper()}/revoke")

    def restore(self, key: str) -> dict:
        return self._request("POST", f"/api/admin/keys/{key.strip().upper()}/restore")

    def reset_hwid(self, key: str) -> dict:
        return self._request("POST", f"/api/admin/keys/{key.strip().upper()}/reset-hwid")
