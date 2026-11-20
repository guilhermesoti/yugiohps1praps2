#!/usr/bin/env python3
"""Capas do OPL com a cara do jogo original e jeito de port de PS2.

    python3 capas_ps2.py <pasta com as artes SLUS_014.11_*.png> <telas: duelo.png carta.png> <saida>

Parte das artes da caixa original (escolha do autor, 20/11/2026) e:
  - capa e verso: troca a faixa "PlayStation" do PS1 por uma faixa preta de
    jogo de PS2 ("PS2 PORT", "PT-BR") e o verso vai para 242x344;
  - logo: tira a faixa branca da borda direita e põe "PORT PARA PS2";
  - ícone: o disco com uma etiqueta "PS2";
  - telas: imagens do port (em português) numa moldura de papiro, como as
    telas do verso da caixa;
  - fundo: o do jogo com as bordas escurecidas.
Tudo com o ID da ISO (RGPM_001.01).
"""
import os
import sys
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

GAME_ID = "RGPM_001.01"
SANS_B = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
SERIF_B = "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"
GOLD, GOLD_DARK = (242, 193, 78), (120, 80, 20)
PS2_BLUE = (0, 70, 160)


def font(path, size):
    return ImageFont.truetype(path, size)


def faixa_ps2(w, h):
    """Faixa preta de capa de PS2, com o filete azul embaixo."""
    bar = Image.new("RGB", (w, h), (8, 8, 10))
    d = ImageDraw.Draw(bar)
    for y in range(h):  # brilho leve de cima
        v = int(28 * (1 - y / h))
        d.line([(0, y), (w, y)], fill=(8 + v, 8 + v, 12 + v))
    d.line([(0, h - 2), (w, h - 2)], fill=PS2_BLUE)
    d.line([(0, h - 1), (w, h - 1)], fill=(0, 40, 100))
    big = font(SANS_B, int(h * 0.62))
    small = font(SANS_B, max(7, int(h * 0.36)))
    x = int(h * 0.35)
    d.text((x, h * 0.44), "PS2", font=big, fill=(255, 255, 255), anchor="lm")
    x += d.textlength("PS2", font=big) + h * 0.22
    d.text((x, h * 0.46), "PORT", font=small, fill=(150, 190, 255), anchor="lm")
    d.text((w - h * 0.35, h * 0.46), "PT-BR", font=small, fill=GOLD, anchor="rm")
    return bar


def arte(img, x0, y0, w, h):
    """Recorta a arte (sem a faixa do PS1) e cobre w x h mantendo a proporção."""
    src = img.crop((x0, y0, img.width, img.height))
    s = max(w / src.width, h / src.height)
    src = src.resize((round(src.width * s), round(src.height * s)), Image.LANCZOS)
    left = (src.width - w) // 2
    top = src.height - h  # o que sobra sai de cima: embaixo ficam o título e a Konami
    return src.crop((left, top, left + w, top + h))


def capa(src, w, h, bar_h, x0, y0):
    img = Image.open(src).convert("RGB")
    up = img.resize((img.width * 4, img.height * 4), Image.LANCZOS)
    out = Image.new("RGB", (w, h))
    out.paste(arte(up, x0 * 4, y0 * 4, w * 4, (h - bar_h) * 4).resize((w, h - bar_h), Image.LANCZOS), (0, bar_h))
    out.paste(faixa_ps2(w, bar_h), (0, 0))
    out = out.filter(ImageFilter.UnsharpMask(radius=1, percent=60, threshold=2))
    return out


def tira_borda_clara(img):
    """Corta colunas quase brancas da direita (e da esquerda)."""
    rgb = img.convert("RGB")
    w, h = rgb.size

    def clara(x):
        px = [rgb.getpixel((x, y)) for y in range(h)]
        return sum(1 for p in px if min(p) > 225) > h * 0.6

    r = w
    while r > w // 2 and clara(r - 1):
        r -= 1
    l = 0
    while l < w // 2 and clara(l):
        l += 1
    # mais uns pontos: a borda clara se mistura com a arte antes de sumir
    return img.crop((l + (2 if l else 0), 0, max(r - 9, w // 2), h))


def logo(src, w=300, h=125):
    img = Image.open(src).convert("RGBA")
    img = tira_borda_clara(img)
    img = img.resize((w, h), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    f = font(SANS_B, 11)
    txt = "PORT PARA PS2"
    tw = d.textlength(txt, font=f)
    x, y = w - tw - 10, 8  # em cima, onde só tem o fundo da arte
    d.rounded_rectangle((x - 6, y - 3, x + tw + 6, y + 14), radius=4, fill=(8, 8, 10, 230), outline=GOLD)
    d.text((x, y - 1), txt, font=f, fill=(255, 255, 255))
    return img


def icone(src):
    img = Image.open(src).convert("RGBA")
    big = img.resize((256, 256), Image.LANCZOS)
    d = ImageDraw.Draw(big)
    f = font(SANS_B, 46)
    d.rounded_rectangle((150, 184, 250, 238), radius=10, fill=(8, 8, 10, 245), outline=PS2_BLUE, width=5)
    d.text((200, 211), "PS2", font=f, fill=(255, 255, 255), anchor="mm")
    return big.resize((64, 64), Image.LANCZOS)


def papiro(bg_src, w, h):
    bg = Image.open(bg_src).convert("RGB")
    # um pedaço só de hieróglifos (o canto de cima, sem o enigma)
    tile = bg.crop((0, 0, 200, 150)).resize((w, h), Image.LANCZOS)
    return ImageEnhance.Brightness(tile).enhance(0.95)


def tela(shot_src, bg_src, w=250, h=188):
    out = papiro(bg_src, w, h)
    shot = Image.open(shot_src).convert("RGB")
    m = 9
    iw, ih = w - 2 * m, h - 2 * m
    shot = shot.resize((iw, ih), Image.LANCZOS)
    d = ImageDraw.Draw(out)
    d.rectangle((m - 3, m - 3, w - m + 2, h - m + 2), fill=(60, 30, 10))
    d.rectangle((m - 2, m - 2, w - m + 1, h - m + 1), outline=(214, 88, 40), width=2)
    out.paste(shot, (m, m))
    return out


def fundo(src):
    img = Image.open(src).convert("RGB").resize((640, 480), Image.LANCZOS)
    mask = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(mask)
    d.ellipse((-160, -120, 800, 600), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(90))
    dark = ImageEnhance.Brightness(img).enhance(0.45)
    return Image.composite(img, dark, mask)


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    src, duelo, carta, out = sys.argv[1:5]
    a = lambda n: os.path.join(src, "SLUS_014.11_%s.png" % n)
    o = lambda n: os.path.join(out, "%s_%s.png" % (GAME_ID, n))
    os.makedirs(out, exist_ok=True)
    # A faixa do PS1 na frente: 19 px em cima e 10 px à esquerda (em 140x200);
    # no verso só a lombada fina à esquerda.
    capa(a("COV"), 140, 200, 18, 10, 19).save(o("COV"))
    capa(a("COV2"), 242, 344, 22, 7, 0).save(o("COV2"))
    logo(a("LGO")).save(o("LGO"))
    icone(a("ICO")).save(o("ICO"))
    tela(duelo, a("BG")).save(o("SCR"))
    tela(carta, a("BG")).save(o("SCR2"))
    fundo(a("BG")).save(o("BG"))
    print("capas em", out)


if __name__ == "__main__":
    main()
