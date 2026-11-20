#!/usr/bin/env python3
"""icon.sys e yfm.icn (o Enigma do Milênio em 3D, pirâmide de 4 faces com o olho; antes uma moeda com o Olho de Rá) da pasta YFMPS2 do memory card (o navegador do PS2 mostra
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


ENIGMA = os.path.join(ROOT, "tools", "ps2", "opl_art", "caixa_original", "SLUS_014.11_BG.png")
# O triângulo do Enigma do Milênio no fundo da caixa (640x480): os dois cantos
# de cima e a ponta de baixo.
TRI = ((80, 222), (522, 222), (300, 455))


def enigma_texture():
    """O Enigma do Milênio (com o olho) recortado da arte, em 128x128: o
    triângulo ocupa a textura toda (cantos de cima em (0,0) e (1,0), ponta
    em (0,5, 1))."""
    from PIL import ImageEnhance
    bg = Image.open(ENIGMA).convert("RGB")
    (x0, y0), (x1, _), (_, y2) = TRI
    im = bg.crop((x0, y0, x1, y2)).resize((128, 128), Image.LANCZOS)
    im = ImageEnhance.Contrast(im).enhance(1.15)
    im = ImageEnhance.Sharpness(im).enhance(1.6)
    return im


def icn_enigma():
    """Prisma triangular: as duas faces com o Enigma e as laterais douradas.
    Maior que a moeda: 4 de largura e uns 2,6 de altura (o SMW tem 2,9 x 2,4)."""
    img = enigma_texture()
    W, T = 2.05, 0.22                        # meia largura, meia espessura
    H = 2 * W * (TRI[2][1] - TRI[0][1]) / (TRI[1][0] - TRI[0][0]) * 1.25  # um pouco mais alto que a arte
    top, tip = -0.35 - H, -0.35              # y para cima é negativo
    A, B, C = (-W, top), (W, top), (0.0, tip)
    uv = {A: (0.01, 0.01), B: (0.99, 0.01), C: (0.5, 0.99)}
    gold = (0.5, 0.18)                       # um ponto dourado da face, para as laterais
    verts = []
    for z, nz, order in ((-T, -1.0, (A, B, C)), (T, 1.0, (A, C, B))):
        for P in order:
            u, v = uv[P]
            if nz > 0: u = 1 - u             # atrás, espelhado (o olho fica certo dos dois lados)
            verts.append((P[0], P[1], z, 0.0, 0.0, nz, u, v))
    for P, Q in ((A, B), (B, C), (C, A)):    # laterais
        ex, ey = Q[0] - P[0], Q[1] - P[1]
        n = (ey, -ex); ln = (n[0] ** 2 + n[1] ** 2) ** 0.5; n = (n[0] / ln, n[1] / ln)
        for (x, y), z in ((P, -T), (Q, -T), (P, T), (Q, -T), (Q, T), (P, T)):
            verts.append((x, y, z, n[0], n[1], 0.0) + gold)
    return img, verts


def icn_piramide():
    """O Enigma do Milênio como ele é: pirâmide de cabeça para baixo, com a
    base quadrada em cima e quatro faces triangulares (o olho em todas)."""
    img = enigma_texture()
    W, top, tip = 1.5, -3.05, -0.15          # meio lado da base; y para cima é negativo
    corners = [(-W, -W), (W, -W), (W, W), (-W, W)]   # (x, z), em volta
    T = (0.0, tip, 0.0)
    gold = (0.5, 0.18)
    verts = []

    def tri(P, Q, R, n, uvs):
        # ordem dos vértices: a mesma da face da frente que o navegador já
        # mostrou certa (o produto vetorial aponta para dentro)
        a = [Q[i] - P[i] for i in range(3)]
        b = [R[i] - P[i] for i in range(3)]
        c = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
        if c[0] * n[0] + c[1] * n[1] + c[2] * n[2] > 0:
            Q, R, uvs = R, Q, (uvs[0], uvs[2], uvs[1])
        for V, uv in zip((P, Q, R), uvs):
            verts.append((V[0], V[1], V[2]) + tuple(n) + tuple(uv))

    for i in range(4):
        (x0, z0), (x1, z1) = corners[i], corners[(i + 1) % 4]
        P, Q = (x0, top, z0), (x1, top, z1)
        mx, mz = (x0 + x1) / 2, (z0 + z1) / 2
        # normal para fora: na direção do meio da aresta, um pouco para baixo
        h = tip - top
        n = (mx * h, -W, mz * h)
        ln = sum(v * v for v in n) ** 0.5
        n = tuple(v / ln for v in n)
        # quem olha a face de fora vê a "direita" em (-nz, 0, nx), como na
        # face da frente (normal -z, direita +x)
        rx, rz = -n[2], n[0]
        left, right = (P, Q) if (x1 - x0) * rx + (z1 - z0) * rz > 0 else (Q, P)
        tri(left, right, T, n, ((0.01, 0.01), (0.99, 0.01), (0.5, 0.99)))
    # a base (em cima), dourada lisa
    A, B, C, D = [(x, top, z) for x, z in corners]
    up = (0.0, -1.0, 0.0)
    tri(A, B, C, up, (gold, gold, gold))
    tri(A, C, D, up, (gold, gold, gold))
    return img, verts


def pack_icn(img, verts):
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
        f.write(pack_icn(*icn_piramide()))
    print("icone em", OUT)


if __name__ == "__main__":
    main()
