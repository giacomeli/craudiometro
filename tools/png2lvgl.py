#!/usr/bin/env python3
"""Converte os PNGs do Craudio em arrays C do LVGL 9 (formato I4).

I4 = indexado 4 bits: paleta de ate 16 cores ARGB8888 seguida dos pixels
(2 por byte, nibble mais significativo primeiro). Um quadro 128x128 ocupa
64 + 8192 bytes em flash.

Gera em firmware/src/assets/:
    craudio_<estado>_<n>.c  - um array por quadro
    craudio_sprites.h       - declaracoes + tabelas de quadros por estado

Uso: python3 tools/png2lvgl.py
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "assets" / "craudio"
OUT_DIR = ROOT / "firmware" / "src" / "assets"

STATES = ("sleep", "idle", "normal", "frantic")


def convert(png_path: Path) -> tuple:
    """Retorna (nome, largura, altura, bytes do mapa I4)."""
    img = Image.open(png_path).convert("RGBA")
    width, height = img.size

    palette = []  # cores RGBA unicas, indice = valor do nibble
    indexed = []
    for y in range(height):
        for x in range(width):
            color = img.getpixel((x, y))
            if color not in palette:
                palette.append(color)
            indexed.append(palette.index(color))
    if len(palette) > 16:
        raise SystemExit(f"{png_path.name}: {len(palette)} cores (maximo 16 no I4)")

    data = bytearray()
    # Paleta: 16 entradas lv_color32_t (B, G, R, A).
    for i in range(16):
        r, g, b, a = palette[i] if i < len(palette) else (0, 0, 0, 0)
        data.extend((b, g, r, a))
    # Pixels: 2 por byte, nibble alto primeiro.
    for y in range(height):
        row = indexed[y * width:(y + 1) * width]
        for i in range(0, width, 2):
            hi = row[i] << 4
            lo = row[i + 1] if i + 1 < width else 0
            data.append(hi | lo)

    return png_path.stem, width, height, bytes(data)


def emit_frame(name: str, width: int, height: int, data: bytes) -> None:
    stride = width // 2
    lines = []
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02x}" for b in data[i:i + 16])
        lines.append(f"    {chunk},")
    body = "\n".join(lines)

    (OUT_DIR / f"{name}.c").write_text(
        "/* Gerado por tools/png2lvgl.py — nao editar manualmente. */\n"
        "#include <lvgl.h>\n\n"
        f"static const uint8_t {name}_map[] = {{\n{body}\n}};\n\n"
        f"const lv_image_dsc_t {name} = {{\n"
        "    .header = {\n"
        "        .magic = LV_IMAGE_HEADER_MAGIC,\n"
        "        .cf = LV_COLOR_FORMAT_I4,\n"
        f"        .w = {width},\n"
        f"        .h = {height},\n"
        f"        .stride = {stride},\n"
        "    },\n"
        f"    .data_size = sizeof({name}_map),\n"
        f"    .data = {name}_map,\n"
        "};\n"
    )


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    pngs = sorted(SRC_DIR.glob("craudio_*.png"))
    if not pngs:
        raise SystemExit(f"nenhum PNG em {SRC_DIR}; rode gen_craudio_sprites.py antes")

    frames = [convert(p) for p in pngs]
    for name, width, height, data in frames:
        emit_frame(name, width, height, data)
        print(f"gerado: {OUT_DIR / (name + '.c')}")

    by_state = {
        state: [n for n, _, _, _ in frames if n.startswith(f"craudio_{state}_")]
        for state in STATES
    }

    decls = "\n".join(
        f"extern const lv_image_dsc_t {name};" for name, _, _, _ in frames
    )
    tables = "\n\n".join(
        f"static const lv_image_dsc_t *const kCraudio{state.capitalize()}[] = {{\n"
        + "\n".join(f"    &{n}," for n in names)
        + f"\n}};\n"
        f"static const uint8_t kCraudio{state.capitalize()}Count = {len(names)};"
        for state, names in by_state.items()
    )

    (OUT_DIR / "craudio_sprites.h").write_text(
        "/* Gerado por tools/png2lvgl.py — nao editar manualmente. */\n"
        "#pragma once\n\n"
        "#include <lvgl.h>\n\n"
        f"{decls}\n\n{tables}\n"
    )
    print(f"gerado: {OUT_DIR / 'craudio_sprites.h'}")


if __name__ == "__main__":
    main()
