#!/usr/bin/env python3
"""icon.sys e yfm.icn da pasta YFMPS2 do memory card (o navegador do PS2 mostra
título e ícone em vez de "dados corrompidos"). O ícone é um quadrado plano
com a arte release/ART/RGPM_001.01_ICO.png, parado.

    python3 gerar_icone.py  ->  src/ps2/mcicon/icon.sys e yfm.icn
"""
import os, struct, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
OUT = os.path.join(ROOT, "src", "ps2", "mcicon")
ART = os.path.join(ROOT, "release", "ART", "RGPM_001.01_ICO.png")


def sjis(text):
    """ASCII para Shift-JIS de largura cheia (o navegador do PS2 espera isso)."""
    out = b""
    for ch in text:
        o = ord(ch)
        if "A" <= ch <= "Z": code = 0x8260 + o - 65
        elif "a" <= ch <= "z": code = 0x8281 + o - 97
        elif "0" <= ch <= "9": code = 0x824F + o - 48
        else: code = {" ": 0x8140, "-": 0x817C, "!": 0x8149, ".": 0x8144, ":": 0x8146, "'": 0x8166, "/": 0x815E}.get(ch, 0x8140)
        out += struct.pack(">H", code)
    return out


def icon_sys(line1, line2, icon_name):
    t1, t2 = sjis(line1), sjis(line2)
    title = (t1 + t2)[:68]
    b = bytearray(964)
    b[0:4] = b"PS2D"
    struct.pack_into("<H", b, 6, len(t1))            # onde começa a 2ª linha
    struct.pack_into("<I", b, 0x0C, 0x40)            # transparência do fundo
    corners = [(30, 20, 70), (30, 20, 70), (10, 10, 40), (10, 10, 40)]
    for i, (r, g, bb) in enumerate(corners):
        struct.pack_into("<4i", b, 0x10 + 16 * i, r, g, bb, 0)
    lights = [(0.0, 0.0, 0.5), (0.0, 0.4, -0.1), (0.0, -0.5, -0.5)]
    for i, v in enumerate(lights):
        struct.pack_into("<4f", b, 0x50 + 16 * i, *v, 0.0)
    colors = [(0.7, 0.7, 0.7), (0.5, 0.5, 0.4), (0.3, 0.3, 0.3)]
    for i, v in enumerate(colors):
        struct.pack_into("<4f", b, 0x80 + 16 * i, *v, 0.0)
    struct.pack_into("<4f", b, 0xB0, 0.5, 0.5, 0.5, 0.0)
    b[0xC0:0xC0 + len(title)] = title
    for off in (0x104, 0x144, 0x184):
        name = icon_name.encode()
        b[off:off + len(name)] = name
    return bytes(b)


def icn(texture_path):
    img = Image.open(texture_path).convert("RGB").resize((128, 128), Image.LANCZOS)
    # quadrado plano, frente para a câmera: 2 triângulos
    s = 0.9
    quad = [(-s, -s, 0, 0, 0), (s, -s, 0, 1, 0), (-s, s, 0, 0, 1),
            (s, -s, 0, 1, 0), (s, s, 0, 1, 1), (-s, s, 0, 0, 1)]
    fx = lambda v: int(round(v * 4096))
    data = bytearray()
    data += struct.pack("<IIIfI", 0x010000, 1, 0x07, 1.0, len(quad))
    for x, y, z, u, v in quad:
        data += struct.pack("<4h", fx(x), fx(y), fx(z), 0)       # posição (shape 0)
        data += struct.pack("<4h", 0, 0, fx(-1.0), 0)            # normal
        data += struct.pack("<2h", fx(u), fx(v))                 # uv
        data += struct.pack("<4B", 0x80, 0x80, 0x80, 0x80)       # cor
    # animação: 1 quadro, sem chaves de movimento
    data += struct.pack("<IIfII", 0x01, 1, 1.0, 0, 1)
    data += struct.pack("<IIII", 0, 1, 0, 0)  # quadro do shape 0, sem chaves extras
    # textura 128x128 A1B5G5R5
    for y in range(128):
        for x in range(128):
            r, g, bb = img.getpixel((x, y))
            data += struct.pack("<H", (r >> 3) | ((g >> 3) << 5) | ((bb >> 3) << 10) | 0x8000)
    return bytes(data)


def main():
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "icon.sys"), "wb") as f:
        f.write(icon_sys("Yu-Gi-Oh! FM", "PS2 Retrogames", "yfm.icn"))
    with open(os.path.join(OUT, "yfm.icn"), "wb") as f:
        f.write(icn(ART))
    print("icone em", OUT)


if __name__ == "__main__":
    main()
