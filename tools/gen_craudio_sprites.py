#!/usr/bin/env python3
"""Gera a spritesheet do Craudinho, releitura pixel-art do mascote do
Claude Code (Clawd, da Anthropic), desenhada do zero neste script a partir de
referencia visual — para uso pessoal no dispositivo.

Cada quadro e desenhado numa grade 16x16 e escalado 8x (vizinho mais proximo)
para 128x128, salvo como PNG RGBA em assets/craudio/.

Estados e quadros:
    sleep_0/1    - dormindo (olhos fechados, balanco lento)
    idle_0/1     - parado (piscada ocasional)
    normal_0..3  - bracos alternando devagar
    frantic_0..3 - bracos agitados + corpo pulando

Uso: python3 tools/gen_craudio_sprites.py
"""

from pathlib import Path

from PIL import Image

OUT_DIR = Path(__file__).resolve().parent.parent / "assets" / "craudio"
TRAY_DIR = Path(__file__).resolve().parent.parent / "assets" / "tray"
SCALE = 8
GRID = 16

PALETTE = {
    ".": (0, 0, 0, 0),          # transparente
    "B": (202, 125, 110, 255),  # corpo salmao do Clawd
    "P": (10, 12, 10, 255),     # olhos
}


def compose(arms: str, eyes: str, legs_phase: int, bob: int) -> list:
    """Monta a grade 16x16 do Craudinho (corpo retangular, olhos verticais,
    bracos laterais com degrau e quatro pernas).

    arms: "up" ou "down"; eyes: "open", "blink" ou "closed";
    legs_phase: 0/1 alterna as pernas; bob: deslocamento vertical do corpo.
    """
    grid = [["."] * GRID for _ in range(GRID)]

    def put(x: int, y: int, ch: str) -> None:
        y += bob
        if 0 <= x < GRID and 0 <= y < GRID:
            grid[y][x] = ch

    def rect(x0: int, y0: int, x1: int, y1: int, ch: str) -> None:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                put(x, y, ch)

    # Corpo: retangulo unico (cabeca + tronco), x=4..11, y=3..10.
    rect(4, 3, 11, 10, "B")

    # Olhos verticais (1x2) sobre o corpo.
    if eyes == "open":
        rect(6, 4, 6, 5, "P")
        rect(9, 4, 9, 5, "P")
    else:  # blink/closed: linha unica
        put(6, 5, "P")
        put(9, 5, "P")

    # Bracos laterais com degrau externo mais baixo (mao).
    arm_top = 4 if arms == "up" else 6
    rect(2, arm_top, 3, arm_top + 2, "B")            # braco esquerdo
    put(2, arm_top + 3, "B")                          # mao esquerda
    rect(12, arm_top, 13, arm_top + 2, "B")           # braco direito
    put(13, arm_top + 3, "B")                         # mao direita

    # Quatro pernas curtas, alternando com a fase.
    for i, x in enumerate((4, 6, 9, 11)):
        drop = 1 if (i + legs_phase) % 2 == 0 else 0
        rect(x, 11, x, 11 + drop, "B")

    return ["".join(row) for row in grid]


FRAMES = {
    "sleep_0": compose("down", "closed", 0, 1),
    "sleep_1": compose("down", "closed", 0, 2),
    "idle_0": compose("down", "open", 0, 0),
    "idle_1": compose("down", "blink", 0, 0),
    "normal_0": compose("up", "open", 0, 0),
    "normal_1": compose("down", "open", 1, 0),
    "normal_2": compose("up", "open", 0, 0),
    "normal_3": compose("down", "open", 1, 0),
    "frantic_0": compose("up", "open", 0, -1),
    "frantic_1": compose("down", "open", 1, 1),
    "frantic_2": compose("up", "open", 1, -1),
    "frantic_3": compose("down", "open", 0, 1),
}


# Variante cinza para o icone de tray "desconectado".
GRAY_PALETTE = {
    ".": (0, 0, 0, 0),
    "B": (158, 158, 158, 255),
    "P": (70, 70, 70, 255),
}


def render(rows: list, scale: int = SCALE, palette: dict = None) -> Image.Image:
    """Renderiza uma grade (qualquer proporcao) com vizinho mais proximo."""
    palette = palette or PALETTE
    width, height = len(rows[0]), len(rows)
    img = Image.new("RGBA", (width, height))
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            img.putpixel((x, y), palette[ch])
    return img.resize((width * scale, height * scale), Image.NEAREST)


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, rows in FRAMES.items():
        path = OUT_DIR / f"craudio_{name}.png"
        render(rows).save(path)
        print(f"gerado: {path}")

    # Icone 32x32 para o header da tela inicial (pose idle, escala 2x).
    icon_path = OUT_DIR / "craudio_icon_0.png"
    idle = compose("down", "open", 0, 0)
    render(idle, scale=2).save(icon_path)
    print(f"gerado: {icon_path}")

    # Icones do tray do daemon (fora do pipeline do firmware): mesma pose
    # idle do mascote, nas cores originais e em cinza.
    TRAY_DIR.mkdir(parents=True, exist_ok=True)
    render(idle, scale=2).save(TRAY_DIR / "craudinho_on.png")
    render(idle, scale=2, palette=GRAY_PALETTE).save(TRAY_DIR / "craudinho_off.png")
    print(f"gerado: {TRAY_DIR / 'craudinho_on.png'}")
    print(f"gerado: {TRAY_DIR / 'craudinho_off.png'}")


if __name__ == "__main__":
    main()
