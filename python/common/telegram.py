"""Telegram Bot API 최소 래퍼. strategy-bot 알림 전송용."""
from __future__ import annotations

import os

import httpx


class TelegramNotifier:
    def __init__(self, bot_token: str | None = None, chat_id: str | None = None):
        self.bot_token = bot_token or os.environ.get("TELEGRAM_BOT_TOKEN", "")
        self.chat_id   = chat_id   or os.environ.get("TELEGRAM_CHAT_ID", "")
        if not self.bot_token or not self.chat_id:
            raise RuntimeError("TELEGRAM_BOT_TOKEN / TELEGRAM_CHAT_ID 미설정")
        self._client = httpx.Client(
            base_url=f"https://api.telegram.org/bot{self.bot_token}",
            timeout=10.0,
        )

    def send(self, text: str) -> None:
        self._client.post(
            "/sendMessage",
            json={"chat_id": self.chat_id, "text": text,
                  "disable_web_page_preview": True},
        )
