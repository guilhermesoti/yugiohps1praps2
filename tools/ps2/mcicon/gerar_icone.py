#!/usr/bin/env python3
"""icon.sys e yfm.icn (moeda 3D com o Olho de Rá) da pasta YFMPS2 do memory card (o navegador do PS2 mostra
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


def eye_of_ra_texture(S=4):
    """Medalha dourada com o Olho de Rá (Wadjet, símbolo do Egito antigo)
    desenhada aqui: 128x128, o disco ocupa a textura toda; o canto (0,0)
    fica dourado liso para a borda da moeda."""
    from PIL import ImageDraw
    w = 128 * S
    im = Image.new("RGB", (w, w), (214, 160, 52))
    d = ImageDraw.Draw(im)
    c = w / 2
    # disco: anéis
    d.ellipse([4 * S, 4 * S, w - 4 * S, w - 4 * S], fill=(150, 98, 24))
    d.ellipse([10 * S, 10 * S, w - 10 * S, w - 10 * S], fill=(236, 184, 70))
    d.ellipse([16 * S, 16 * S, w - 16 * S, w - 16 * S], outline=(170, 112, 30), width=2 * S)
    # brilho
    d.ellipse([26 * S, 22 * S, 62 * S, 46 * S], fill=(250, 214, 120))
    ink, lapis = (60, 30, 10), (30, 60, 140)
    L = lambda pts: [(x * S, y * S) for x, y in pts]
    # sobrancelha
    d.line(L([(30, 46), (48, 40), (72, 39), (98, 44)]), fill=ink, width=5 * S, joint="curve")
    # olho (amêndoa)
    d.polygon(L([(30, 60), (44, 52), (64, 49), (84, 52), (100, 60), (84, 66), (64, 69), (44, 66)]), fill=(250, 240, 220), outline=ink)
    d.line(L([(30, 60), (44, 52), (64, 49), (84, 52), (100, 60)]), fill=ink, width=4 * S)
    d.line(L([(30, 60), (44, 66), (64, 69), (84, 66), (100, 60)]), fill=ink, width=3 * S)
    # pupila
    d.ellipse(L([(56, 50), (74, 68)]), fill=lapis, outline=ink, width=2 * S)
    d.ellipse(L([(62, 55), (68, 61)]), fill=(240, 240, 255))
    # traço que segue o canto do olho
    d.line(L([(100, 60), (110, 60)]), fill=ink, width=4 * S)
    # marca vertical (lágrima)
    d.line(L([(58, 68), (56, 84), (54, 98)]), fill=ink, width=5 * S)
    # espiral
    d.line(L([(70, 68), (74, 80), (80, 90), (88, 94), (94, 90), (93, 83), (86, 82), (84, 87)]), fill=ink, width=4 * S, joint="curve")
    im = im.resize((128, 128), Image.LANCZOS)
    im.putpixel((0, 0), (214, 160, 52)); im.putpixel((1, 0), (214, 160, 52)); im.putpixel((0, 1), (214, 160, 52))
    return im


def icn(texture_path=None):
    """Moeda 3D: duas faces com o olho e a borda grossa dourada."""
    import math
    img = eye_of_ra_texture()
    N, R, T, CY = 24, 1.0, 0.18, -1.05      # lados, raio, meia espessura, centro (y para cima é negativo)
    verts = []                               # (x, y, z, nx, ny, nz, u, v)
    def ang(i): return 2 * math.pi * i / N
    for side, z, nz in ((0, -T, -1.0), (1, T, 1.0)):
        for i in range(N):
            a0, a1 = ang(i), ang(i + 1)
            pts = [(0.0, 0.0), (math.cos(a0), math.sin(a0)), (math.cos(a1), math.sin(a1))]
            if side: pts = [pts[0], pts[2], pts[1]]
            for px, py in pts:
                u = 0.5 + 0.47 * (px if side == 0 else -px)
                v = 0.5 + 0.47 * py
                verts.append((px * R, CY + py * R, z, 0.0, 0.0, nz, u, v))
    for i in range(N):                       # borda
        a0, a1 = ang(i), ang(i + 1)
        c0, s0, c1, s1 = math.cos(a0), math.sin(a0), math.cos(a1), math.sin(a1)
        q = [(c0, s0, -T), (c1, s1, -T), (c0, s0, T), (c1, s1, -T), (c1, s1, T), (c0, s0, T)]
        for cx, sy, z in q:
            verts.append((cx * R, CY + sy * R, z, cx, sy, 0.0, 0.004, 0.004))
    fx = lambda v: int(round(v * 4096))
    data = bytearray()
    data += struct.pack("<IIIfI", 0x010000, 1, 0x07, 1.0, len(verts))
    for x, y, z, nx, ny, nz, u, v in verts:
        data += struct.pack("<4h", fx(x), fx(y), fx(z), 0)
        data += struct.pack("<4h", fx(nx), fx(ny), fx(nz), 0)
        data += struct.pack("<2h", fx(u), fx(v))
        data += struct.pack("<4B", 0x80, 0x80, 0x80, 0x80)
    data += struct.pack("<IIfII", 0x01, 1, 1.0, 0, 1)
    data += struct.pack("<IIII", 0, 1, 0, 0)
    for y in range(128):
        for x in range(128):
            r, g, bb = img.getpixel((x, y))
            data += struct.pack("<H", (r >> 3) | ((g >> 3) << 5) | ((bb >> 3) << 10) | 0x8000)
    img.save(os.path.join(OUT, "textura_preview.png"))
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
