#!/usr/bin/env python3
"""Captura screenshots reais do Craudiometro pela porta serial.

O firmware, ao receber "shot\\n", redesenha a tela inteira e transmite cada
faixa renderizada no protocolo SNAP:

    SNAP BEGIN <largura> <altura>
    SNAP AREA <x> <y> <w> <h>    (seguido de w*h*2 bytes RGB565 little-endian)
    ...
    SNAP END

O comando "screen <nome>\\n" troca a tela antes da captura (nomes do registro:
dashboard, craudinho, configuracoes, menu).

Uso:
    python3 tools/screenshot.py --out dashboard.png
    python3 tools/screenshot.py --screen craudinho --out craudinho.png
    python3 tools/screenshot.py --all --out-dir docs/screenshots

Dependencias: pyserial e Pillow.

Nota sobre reset: o driver do CH340 assere DTR ao abrir a porta, o que segura
o ESP32 em reset (circuito de auto-programacao do DevKit). Nao ha como abrir
sem perturbar o chip; entao o script solta DTR/RTS logo apos abrir (o chip
reboota e roda), espera o banner de boot e, por padrao, espera tambem o
primeiro payload BLE do daemon para capturar com dados reais (--no-wait pula
essa espera). Dica: reinicie o daemon (launchctl kickstart) apos o boot para
acelerar a sincronizacao.
"""

import argparse
import glob
import sys
import time

import serial
from PIL import Image

BAUD = 115200
WIDTH, HEIGHT = 240, 320
ALL_SCREENS = ("dashboard", "craudinho", "configuracoes", "menu")


def find_port() -> str:
    ports = sorted(glob.glob("/dev/cu.usbserial*"))
    if not ports:
        sys.exit("nenhuma porta /dev/cu.usbserial* encontrada")
    return ports[0]


def wait_for(ser: serial.Serial, needle: str, timeout_s: float) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        line = ser.readline().decode(errors="replace")
        if needle in line:
            return True
    return False


def open_port(port: str) -> serial.Serial:
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = BAUD
    ser.timeout = 5
    ser.open()
    # O open assere DTR (CH340) e o chip fica em reset: solta as linhas para
    # ele rebootar em modo normal e espera o boot terminar.
    ser.dtr = False
    ser.rts = False
    if not wait_for(ser, "prontos", 15):
        sys.exit("dispositivo nao respondeu ao boot (banner ausente)")
    return ser


def capture_cycle(ser: serial.Serial, canvas: bytearray, covered: bytearray) -> None:
    """Um ciclo de shot: mescla as faixas recebidas no canvas e marca a
    cobertura. O firmware transmite um ciclo de refresh por shot, e o LVGL
    pode dividir o redesenho entre ciclos conforme as animacoes invalidam
    areas — por isso a captura completa pode exigir mais de um shot."""
    ser.reset_input_buffer()
    ser.write(b"shot\n")

    deadline = time.time() + 90
    while time.time() < deadline:
        line = ser.readline().decode(errors="replace").strip()
        if line.startswith("SNAP AREA "):
            _, _, x, y, w, h = line.split()
            x, y, w, h = int(x), int(y), int(w), int(h)
            data = ser.read(w * h * 2)
            if len(data) != w * h * 2:
                return  # faixa incompleta: descarta o resto deste ciclo
            for row in range(h):
                offset = (y + row) * WIDTH + x
                canvas[offset * 2:(offset + w) * 2] = \
                    data[row * w * 2:(row + 1) * w * 2]
                covered[offset:offset + w] = b"\x01" * w
        elif line.startswith("SNAP END"):
            return
        # Outras linhas (logs do firmware) sao ignoradas.
    sys.exit("timeout esperando SNAP END")


def capture(ser: serial.Serial) -> bytearray:
    """Repete shots ate cobrir a tela inteira (maximo 5 tentativas)."""
    canvas = bytearray(WIDTH * HEIGHT * 2)
    covered = bytearray(WIDTH * HEIGHT)
    for attempt in range(5):
        capture_cycle(ser, canvas, covered)
        missing = covered.count(0)
        if missing == 0:
            return canvas
        print(f"  cobertura parcial ({missing} px faltando); novo shot...")
        time.sleep(0.3)
    sys.exit("nao foi possivel cobrir a tela inteira em 5 tentativas")


def to_image(canvas: bytearray) -> Image.Image:
    img = Image.new("RGB", (WIDTH, HEIGHT))
    pixels = img.load()
    for i in range(WIDTH * HEIGHT):
        value = canvas[2 * i] | (canvas[2 * i + 1] << 8)
        r = (value >> 11) & 0x1F
        g = (value >> 5) & 0x3F
        b = value & 0x1F
        pixels[i % WIDTH, i // WIDTH] = (
            (r * 255) // 31, (g * 255) // 63, (b * 255) // 31)
    return img


def switch_screen(ser: serial.Serial, name: str) -> None:
    ser.write(f"screen {name}\n".encode())
    time.sleep(1.5)  # transicao de tela + primeiro refresh


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=None, help="porta serial (auto)")
    parser.add_argument("--screen", default=None, help="tela a capturar")
    parser.add_argument("--out", default="screenshot.png")
    parser.add_argument("--all", action="store_true",
                        help="captura todas as telas em --out-dir")
    parser.add_argument("--out-dir", default="docs/screenshots")
    parser.add_argument("--no-wait", action="store_true",
                        help="nao espera o primeiro payload BLE do daemon")
    args = parser.parse_args()

    ser = open_port(args.port or find_port())
    try:
        if not args.no_wait:
            print("aguardando primeiro payload BLE do daemon (ate 100 s)...")
            if not wait_for(ser, "payload aplicado", 100):
                print("aviso: sem payload; capturando sem dados sincronizados")
        if args.all:
            from pathlib import Path
            out_dir = Path(args.out_dir)
            out_dir.mkdir(parents=True, exist_ok=True)
            for name in ALL_SCREENS:
                switch_screen(ser, name)
                to_image(capture(ser)).save(out_dir / f"{name}.png")
                print(f"capturado: {out_dir / (name + '.png')}")
            switch_screen(ser, ALL_SCREENS[0])  # volta para a inicial
        else:
            if args.screen:
                switch_screen(ser, args.screen)
            to_image(capture(ser)).save(args.out)
            print(f"capturado: {args.out}")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
