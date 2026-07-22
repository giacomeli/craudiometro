#!/usr/bin/env python3
"""Daemon do Craudiômetro (macOS).

Le o token OAuth do Claude Code no Keychain, consulta o endpoint de uso da
Anthropic (sem custo de tokens) e envia o resultado ao dispositivo ESP32 via
BLE (GATT write) a cada 60 segundos.

Payload enviado ao dispositivo:
    { "s": 45, "sr": 120, "w": 28, "wr": 7200, "st": "allowed", "ok": true }

    s  - utilizacao da sessao 5h (percentual inteiro)
    sr - minutos ate o reset da sessao
    w  - utilizacao da semana (percentual inteiro)
    wr - minutos ate o reset semanal
    st - status derivado da maior severidade dos limites ("allowed" se normal)
    ok - coleta na API bem-sucedida
    pl - nome do plano da conta (subscriptionType capitalizado, ex.: "Max")
    tm - minutos desde a meia-noite no fuso local (base do relogio do device)
"""

import asyncio
import json
import logging
import logging.handlers
import subprocess
import threading
from datetime import datetime, timezone
from pathlib import Path

import httpx
from bleak import BleakClient, BleakScanner

KEYCHAIN_SERVICE = "Claude Code-credentials"
USAGE_URL = "https://api.anthropic.com/api/oauth/usage"
OAUTH_BETA = "oauth-2025-04-20"

DEVICE_NAME = "craudiometro"
SERVICE_UUID = "6a485858-14a2-4918-9841-d3c2d2474fbc"
RX_CHAR_UUID = "23aefc82-8cba-470b-ba8f-d1c54f9f0656"

POLL_INTERVAL_S = 60
SCAN_TIMEOUT_S = 15
BACKOFF_MIN_S = 5
BACKOFF_MAX_S = 300

LOG_PATH = Path.home() / "Library" / "Logs" / "craudiometro.log"

log = logging.getLogger("craudiometro")

# Estado compartilhado entre a thread BLE e o app de tray (dict + GIL bastam
# para leituras/escritas atomicas de valores simples).
state = {"connected": False, "paused": False, "last_sent": "", "summary": ""}


def find_tray_icon(name: str):
    """Icone ao lado do daemon instalado, ou em assets/tray/ no repo."""
    here = Path(__file__).resolve().parent
    for base in (here, here.parent / "assets" / "tray"):
        path = base / name
        if path.exists():
            return str(path)
    return None


def setup_logging() -> None:
    LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
    handler = logging.handlers.RotatingFileHandler(
        LOG_PATH, maxBytes=512 * 1024, backupCount=3
    )
    fmt = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
    handler.setFormatter(fmt)
    console = logging.StreamHandler()
    console.setFormatter(fmt)
    log.addHandler(handler)
    log.addHandler(console)
    log.setLevel(logging.INFO)


def read_credentials() -> tuple:
    """Le (accessToken, plano) do Keychain. Levanta em caso de falha."""
    raw = subprocess.check_output(
        ["security", "find-generic-password", "-s", KEYCHAIN_SERVICE, "-w"],
        text=True,
    ).strip()
    oauth = json.loads(raw)["claudeAiOauth"]
    plan = (oauth.get("subscriptionType") or "").capitalize()
    return oauth["accessToken"], plan


def local_minutes() -> int:
    """Minutos desde a meia-noite no fuso local (campo `tm` do payload)."""
    now = datetime.now().astimezone()
    return now.hour * 60 + now.minute


def minutes_until(iso_ts: str) -> int:
    when = datetime.fromisoformat(iso_ts)
    delta = (when - datetime.now(timezone.utc)).total_seconds() / 60
    return max(0, round(delta))


def build_payload() -> dict:
    """Consulta a API e monta o payload. Em falha, retorna ok=false com a
    hora local mesmo assim (o relogio do dispositivo segue corrigido)."""
    try:
        token, plan = read_credentials()
    except (subprocess.CalledProcessError, KeyError, json.JSONDecodeError) as err:
        log.warning("falha ao ler o Keychain: %s", err)
        return {"ok": False, "tm": local_minutes()}

    try:
        resp = httpx.get(
            USAGE_URL,
            headers={
                "Authorization": f"Bearer {token}",
                "anthropic-beta": OAUTH_BETA,
                "User-Agent": "craudiometro-daemon/1.0",
            },
            timeout=20,
        )
        resp.raise_for_status()
        usage = resp.json()
    except httpx.HTTPError as err:
        log.warning("falha na API de uso: %s", err)
        return {"ok": False, "tm": local_minutes(), "pl": plan}

    try:
        five_hour = usage["five_hour"]
        seven_day = usage["seven_day"]

        severities = [
            limit.get("severity", "normal")
            for limit in usage.get("limits", [])
        ]
        abnormal = [s for s in severities if s != "normal"]
        status = abnormal[0] if abnormal else "allowed"

        return {
            "s": round(five_hour["utilization"]),
            "sr": minutes_until(five_hour["resets_at"]),
            "w": round(seven_day["utilization"]),
            "wr": minutes_until(seven_day["resets_at"]),
            "st": status,
            "ok": True,
            "pl": plan,
            "tm": local_minutes(),
        }
    except (KeyError, TypeError, ValueError) as err:
        log.warning("resposta da API em formato inesperado: %s", err)
        return {"ok": False, "tm": local_minutes(), "pl": plan}


async def run_connected(client: BleakClient) -> None:
    """Envia o payload a cada POLL_INTERVAL_S enquanto conectado e ativo."""
    state["connected"] = True
    while client.is_connected and not state["paused"]:
        payload = build_payload()
        data = json.dumps(payload, separators=(",", ":")).encode()
        await client.write_gatt_char(RX_CHAR_UUID, data, response=True)
        log.info("enviado: %s", payload)
        state["last_sent"] = datetime.now().strftime("%H:%M")
        if payload.get("ok"):
            state["summary"] = f"sessao {payload['s']}% / semana {payload['w']}%"
        # Sleep em passos de 1 s para reagir rapido a pausa/desconexao.
        for _ in range(POLL_INTERVAL_S):
            if state["paused"] or not client.is_connected:
                break
            await asyncio.sleep(1)


async def ble_main() -> None:
    log.info("daemon iniciado; procurando '%s'", DEVICE_NAME)
    backoff = BACKOFF_MIN_S

    while True:
        if state["paused"]:
            await asyncio.sleep(2)
            continue
        try:
            # Busca pelo service UUID (imune a limites do pacote de adv);
            # o nome fica como criterio de fallback.
            device = await BleakScanner.find_device_by_filter(
                lambda d, adv: SERVICE_UUID in (adv.service_uuids or [])
                or (adv.local_name or "") == DEVICE_NAME,
                timeout=SCAN_TIMEOUT_S,
            )
            if device is None:
                raise RuntimeError("dispositivo nao encontrado no scan")

            async with BleakClient(device) as client:
                log.info("conectado a %s", device.address)
                backoff = BACKOFF_MIN_S
                await run_connected(client)
            state["connected"] = False
            log.info("desconectado%s", " (pausado)" if state["paused"] else "")
        except asyncio.CancelledError:
            raise
        except Exception as err:  # noqa: BLE001 - loop de resiliencia
            state["connected"] = False
            log.warning("ciclo falhou (%s); nova tentativa em %ds", err, backoff)
            await asyncio.sleep(backoff)
            backoff = min(backoff * 2, BACKOFF_MAX_S)


def run_tray() -> None:
    """App de menu bar (rumps). Roda na thread principal; o icone e o menu
    refletem o estado da thread BLE via timer de 2 s."""
    import rumps

    class CraudioTray(rumps.App):
        def __init__(self):
            super().__init__("Craudiômetro", icon=find_tray_icon("craudinho_off.png"),
                             quit_button=None)
            self.statusItem = rumps.MenuItem("iniciando...")
            self.sentItem = rumps.MenuItem("ultimo envio: --")
            self.logItem = rumps.MenuItem("Ver log", callback=self.openLog)
            self.pauseItem = rumps.MenuItem("Pausar", callback=self.togglePause)
            self.menu = [self.statusItem, self.sentItem, None, self.logItem,
                         self.pauseItem]
            self.currentIcon = "off"
            self.refreshTimer = rumps.Timer(self.refresh, 2)
            self.refreshTimer.start()

        def refresh(self, _):
            if state["paused"]:
                status = "Pausado"
            elif state["connected"]:
                summary = state["summary"]
                status = f"Conectado — {summary}" if summary else "Conectado"
            else:
                status = "Desconectado"
            self.statusItem.title = status
            self.sentItem.title = f"ultimo envio: {state['last_sent'] or '--'}"
            self.pauseItem.title = "Retomar" if state["paused"] else "Pausar"

            wanted = "on" if (state["connected"] and not state["paused"]) else "off"
            if wanted != self.currentIcon:
                self.currentIcon = wanted
                self.icon = find_tray_icon(f"craudinho_{wanted}.png")

        def togglePause(self, _):
            state["paused"] = not state["paused"]
            log.info("pausado pelo tray" if state["paused"] else "retomado pelo tray")

        def openLog(self, _):
            subprocess.run(["open", str(LOG_PATH)], check=False)

    CraudioTray().run()


def run_ble_thread() -> None:
    try:
        asyncio.run(ble_main())
    except Exception:  # noqa: BLE001 - nao derrubar o tray silenciosamente
        log.exception("loop BLE terminou de forma inesperada")


if __name__ == "__main__":
    setup_logging()
    try:
        import rumps  # noqa: F401
        hasTray = find_tray_icon("craudinho_on.png") is not None
    except Exception:  # noqa: BLE001 - sessao sem UI ou dependencia ausente
        hasTray = False

    try:
        if hasTray:
            threading.Thread(target=run_ble_thread, daemon=True).start()
            run_tray()
        else:
            log.info("tray indisponivel (sem rumps ou icones); rodando headless")
            asyncio.run(ble_main())
    except KeyboardInterrupt:
        pass
