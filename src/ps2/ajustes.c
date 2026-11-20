/* Telas AJUSTES e "A imagem apareceu?" do port no PS2, no mesmo visual e
 * com o mesmo jeito de usar do port de Super Mario World (formato.c de lá):
 * fundo azul com estrelas, moldura dourada dupla, fonte 8x8 do PS2SDK com
 * os acentos do português, linhas com setas, painel de ajuda e o botão
 * JOGAR. Desenhadas num quadro 320 x 240 de 32 bits (gs_gpu.c mostra).
 *
 * O círculo do teste da tela só fica redondo quando a TV está no modo certo
 * para a opção marcada: no 16:9 ele é desenhado espremido, e a TV em modo
 * Wide (que estica a imagem 4:3 em 4/3) o deixa redondo de novo. */
#include "ajustes.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define W AJUSTES_W
#define H AJUSTES_H
#define OY 8 /* o layout do port do Mario é de 224 linhas: centrado nas 240 */

static uint32_t *px;
static const unsigned char *fonte;

enum {
  DOURADO = 0xF2C14E, AZUL_CLARO = 0xA9C8FF, CINZA = 0x8890B0, TEXTO = 0xC8D0F0,
  BRANCO = 0xFFFFFF, CAIXA = 0x080C26, BORDA = 0x34407A, LARANJA = 0xFF9A4A
};

static void fill(int x, int y, int w, int h, uint32_t rgb) {
  for (int j = y; j < y + h; j++)
    if ((unsigned)j < H)
      for (int i = x; i < x + w; i++)
        if ((unsigned)i < W) px[j * W + i] = rgb;
}

/* Pinta por cima com transparência (a = 0..256). */
static void mistura(int x, int y, int w, int h, uint32_t rgb, int a) {
  for (int j = y; j < y + h; j++)
    if ((unsigned)j < H)
      for (int i = x; i < x + w; i++) {
        if ((unsigned)i >= W) continue;
        uint32_t p = px[j * W + i], out = 0;
        for (int s = 0; s < 24; s += 8) {
          int c = (int)((p >> s) & 255), n = (int)((rgb >> s) & 255);
          out |= (uint32_t)(c + (n - c) * a / 256) << s;
        }
        px[j * W + i] = out;
      }
}

static void contorno(int x, int y, int w, int h, int b, uint32_t rgb) {
  fill(x, y, w, b, rgb), fill(x, y + h - b, w, b, rgb);
  fill(x, y, b, h, rgb), fill(x + w - b, y, b, h, rgb);
}

/* ---- texto: fonte 8x8 do PS2SDK, 6 pixels por letra ----
   Os acentos do português estão na metade de cima da fonte (MSX). */
static int glifo(const char **p) {
  static const unsigned char tabela[][2] = {
      {0xA1, 0xA0}, {0xA0, 0x85}, {0xA2, 0x83}, {0xA3, 0xB1}, {0xA7, 0x87}, {0xA9, 0x82},
      {0xAA, 0x88}, {0xAD, 0xA1}, {0xB3, 0xA2}, {0xB4, 0x93}, {0xB5, 0xB5}, {0xBA, 0xA3},
      {0xBC, 0x81}, {0x87, 0x80}, {0x89, 0x90}, {0x83, 0xB0}, {0x95, 0xB4}};
  const unsigned char *s = (const unsigned char *)*p;
  int c = *s++;
  if (c == 0xC3 && *s) {
    int u = *s++;
    c = '?';
    for (size_t i = 0; i < sizeof(tabela) / sizeof(tabela[0]); i++)
      if (tabela[i][0] == u) c = tabela[i][1];
  }
  *p = (const char *)s;
  return c;
}

static int largura(const char *s, int escala) {
  int n = 0;
  while (*s) glifo(&s), n++;
  return n ? n * 6 * escala - escala : 0;
}

static void texto(int x, int y, const char *s, uint32_t rgb, int escala) {
  while (*s) {
    const unsigned char *g = fonte + glifo(&s) * 8;
    for (int r = 0; r < 8; r++)
      for (int c = 0; c < 6; c++)
        if (g[r] & (0x80 >> c)) fill(x + c * escala, y + r * escala, escala, escala, rgb);
    x += 6 * escala;
  }
}

/* Centrado na faixa [x0, x0 + w). */
static void texto_em(int x0, int w, int y, const char *s, uint32_t rgb) {
  texto(x0 + (w - largura(s, 1)) / 2, y, s, rgb, 1);
}

static void texto_sombra(int y, const char *s, uint32_t rgb, int escala) {
  int x = (W - largura(s, escala)) / 2;
  texto(x + 1, y + 1, s, 0x000001, escala);
  texto(x, y, s, rgb, escala);
}

/* ---- fundo: o mesmo do port do Mario ---- */
static unsigned semente;
static float sorteio(void) {
  semente = semente * 1103515245u + 12345u;
  return (float)((semente >> 8) & 0xFFFF) / 65535.0f;
}

static void fundo(int frame) {
  for (int y = 0; y < H; y++) {
    uint32_t c = (uint32_t)(10 + y * 14 / H) << 16 | (uint32_t)(12 + y * 6 / H) << 8 | (uint32_t)(40 + y * 30 / H);
    for (int x = 0; x < W; x++) px[y * W + x] = c;
  }
  semente = 777;
  for (int i = 0; i < 60; i++) {
    int sx = (int)(sorteio() * W), sy = (int)(sorteio() * H), tw = (int)(sorteio() * 60);
    int k = 40 + (int)(40 * (1 + sinf((frame + tw * 7) * 0.05f)));
    mistura((sx + frame / 8) % W, sy, 1, 1, (uint32_t)k << 16 | (uint32_t)k << 8 | (uint32_t)(k + 20), 200);
  }
  contorno(10, 8, W - 20, H - 16, 2, 0xC8962E);
  contorno(15, 13, W - 30, H - 26, 1, 0x7A5A1C);
}

/* ---- desenhos ---- */

/* TV pequena com um campo de duelo: o céu, a mesa verde e as cartas. larga:
   tela 16:9, com as faixas a mais (a imagem larga tem 54 colunas a mais de
   cada lado das 320) marcadas em dourado. */
static void tv(int x, int y, bool larga) {
  int sw = larga ? 44 : 33, sh = 24, tw = sw + 6, th = sh + 6;
  int sx = x + 3, sy = y + 3, mesa = sy + sh / 2;
  fill(x, y, tw, th, 0x5A6070);
  fill(x + 1, y + 1, tw - 2, th - 2, 0x2B2F3A);
  for (int j = 0; j < sh / 2; j++) fill(sx, sy + j, sw, 1, (uint32_t)(0x18 + j * 3) << 8 | (uint32_t)(0x40 + j * 6));
  fill(sx, mesa, sw, sh - sh / 2, 0x245A30);
  for (int i = 0; i < 5; i++) fill(sx + sw / 2 - 12 + i * 5, mesa + 3, 3, 5, i == 2 ? 0xF8D878 : 0xC8A050);
  for (int i = 0; i < 5; i++) fill(sx + sw / 2 - 12 + i * 5, sy + 3, 3, 4, 0x8A6A3A);
  if (larga) {
    int faixa = sw * 54 / 428 + 1;
    mistura(sx, sy, faixa, sh, DOURADO, 90);
    mistura(sx + sw - faixa, sy, faixa, sh, DOURADO, 90);
  }
  fill(x + tw / 2 - 5, y + th, 10, 2, 0x2B2F3A);
}

/* Contorno de elipse com 2 pixels de espessura. */
static void elipse(int cx, int cy, float rx, float ry, uint32_t cor) {
  for (int k = 0; k < 2; k++)
    for (int i = 0; i < 720; i++) {
      float a = (float)i * 3.14159265f / 360.0f;
      fill(cx + (int)lroundf((rx - k) * cosf(a)), cy + (int)lroundf((ry - k) * sinf(a)), 1, 1, cor);
    }
}

/* Triângulo apontando para a esquerda (dir < 0) ou para a direita. */
static void seta(int x, int cy, int dir, uint32_t cor) {
  for (int i = 0; i < 4; i++) {
    int h = 2 * i + 1, cx = dir < 0 ? x + i : x + 3 - i;
    fill(cx, cy - i, 1, h, cor);
  }
}

/* O olho de Hórus pequeno, em dourado, dos dois lados do título. */
static void olho(int x, int y) {
  fill(x, y, 10, 1, DOURADO);
  fill(x + 1, y + 2, 8, 1, DOURADO);
  fill(x, y + 3, 1, 1, DOURADO), fill(x + 9, y + 3, 1, 1, DOURADO);
  fill(x + 1, y + 4, 8, 1, DOURADO);
  fill(x + 4, y + 2, 2, 3, DOURADO);
  fill(x + 3, y + 5, 1, 3, DOURADO);
  fill(x + 4, y + 8, 3, 1, DOURADO);
}

/* ---- AJUSTES ---- */

static const char *const kRotulo[4] = {"Vídeo", "Tela", "Suavização", "Idioma"};
static const char *const kIdioma[2] = {"Português", "English"};
static const char *const kSuave[3] = {"Desligada", "Média", "Forte"};
static const char *const kVideoTxt[3][2] = {
    {"240p: cabo AV, em qualquer TV.", "A imagem mais parecida com o PS1."},
    {"480p: cabo componente ou adaptador HDMI.", "Se a TV não mostrar, volta sozinho."},
    {"480i: cabo AV em TV de tela plana.", "Experimente se o 240p ficar ruim."}};
static const char *const kTelaTxt[2] = {"4:3: TV de tubo ou em 4:3.", "16:9: mostra mais do duelo."};
static const char *const kSuaveTxt[3][2] = {
    {"Igual ao PS1, com o pontilhado.", "Pixels nítidos, como no console."},
    {"Sem o pontilhado do PS1.", "Cores limpas, sem granulado."},
    {"Sem pontilhado e com o 3D suave.", "Monstros e campo sem serrilhado."}};

enum { LX = 28, LW = W - 56, LINHA_Y = 42 + OY, LINHA_H = 16, LINHA_PASSO = 18, VALOR_X = 152 };

static const char *valor(const Ajustes *a, int linha) {
  static const char *const videos[] = {"240p", "480p", "480i"};
  switch (linha) {
  case AJUSTE_VIDEO: return videos[a->video];
  case AJUSTE_TELA: return a->tela ? "16:9" : "4:3";
  case AJUSTE_SUAVE: return kSuave[a->suave];
  default: return kIdioma[a->idioma];
  }
}

static void linha_ajuste(const Ajustes *a, int linha, int frame) {
  int y = LINHA_Y + linha * LINHA_PASSO;
  bool marcada = a->linha == linha;
  fill(LX, y, LW, LINHA_H, CAIXA);
  contorno(LX, y, LW, LINHA_H, marcada ? 2 : 1, marcada ? DOURADO : BORDA);
  texto(LX + 9, y + 4, kRotulo[linha], marcada ? DOURADO : AZUL_CLARO, 1);
  int fim = LX + LW - 8;
  texto_em(VALOR_X, fim - VALOR_X, y + 4, valor(a, linha), marcada ? BRANCO : TEXTO);
  uint32_t cor = marcada ? DOURADO : BORDA;
  int pulo = marcada && (frame / 15) % 2 ? 1 : 0;
  seta(VALOR_X + 2 - pulo, y + LINHA_H / 2, -1, cor);
  seta(fim - 6 + pulo, y + LINHA_H / 2, 1, cor);
}

static void botao_jogar(const Ajustes *a, int frame) {
  int w = 96, x = (W - w) / 2, y = LINHA_Y + 4 * LINHA_PASSO + 4;
  bool marcado = a->linha == AJUSTE_JOGAR;
  fill(x, y, w, LINHA_H, marcado ? 0x3A2A08 : CAIXA);
  contorno(x, y, w, LINHA_H, marcado ? 2 : 1, marcado ? DOURADO : BORDA);
  texto_em(x, w, y + 4, "JOGAR", marcado ? DOURADO : AZUL_CLARO);
  if (marcado && (frame / 20) % 2 == 0) seta(x - 9, y + LINHA_H / 2, 1, DOURADO), seta(x + w + 5, y + LINHA_H / 2, -1, DOURADO);
}

/* Painel de ajuda da linha marcada. */
static void painel(const Ajustes *a) {
  const int x = LX, y = 138 + OY, w = LW, h = 42;
  fill(x, y, w, h, CAIXA);
  contorno(x, y, w, h, 1, BORDA);
  int tx = x + 8, l1 = y + 6, l2 = y + 17, l3 = y + 28;
  switch (a->linha) {
  case AJUSTE_VIDEO:
    texto(tx, l1, kVideoTxt[a->video][0], BRANCO, 1);
    texto(tx, l2, kVideoTxt[a->video][1], TEXTO, 1);
    texto(tx, l3, "Muda quando você apertar X.", CINZA, 1);
    break;
  case AJUSTE_TELA: {
    tv(x + 6, y + 6, a->tela);
    /* O quadro tem 320 x 240 pontos numa TV 4:3: pontos quadrados. Na TV
       em modo 16:9 a imagem é esticada 4/3, então no 16:9 o círculo é
       desenhado 3/4 mais estreito para voltar a ficar redondo. */
    const float ry = 13.0f, rx = a->tela ? ry * 0.75f : ry;
    int cx = x + 76, cy = y + h / 2;
    elipse(cx, cy, rx, ry, DOURADO);
    fill(cx - 1, cy - 1, 3, 3, DOURADO);
    texto(x + 98, l1, kTelaTxt[a->tela], BRANCO, 1);
    texto(x + 98, l2, "O círculo fica redondo", TEXTO, 1);
    texto(x + 98, l3, "com a TV no modo certo.", TEXTO, 1);
    break;
  }
  case AJUSTE_SUAVE:
    texto(tx, l1, kSuaveTxt[a->suave][0], BRANCO, 1);
    texto(tx, l2, kSuaveTxt[a->suave][1], TEXTO, 1);
    texto(tx, l3, "Vale para o jogo, não para esta tela.", CINZA, 1);
    break;
  case AJUSTE_IDIOMA:
    if (a->sem_traducao) {
      texto(tx, l1, "Só em inglês: falta a tradução.", BRANCO, 1);
      texto(tx, l2, "Crie o port de novo pelo criar-port", TEXTO, 1);
      texto(tx, l3, "ou ponha languages/pt-br.txt no ELF.", TEXTO, 1);
    } else {
      texto(tx, l1, a->idioma ? "Textos do jogo em inglês (original)." : "Textos do jogo em português.", BRANCO, 1);
      texto(tx, l2, "Diálogos, cartas e menus.", TEXTO, 1);
      texto(tx, l3, "Vale para o jogo, não para esta tela.", CINZA, 1);
    }
    break;
  default:
    texto(tx, l1, "X ou START começa. Fica tudo salvo.", BRANCO, 1);
    texto(tx, l2, "No jogo, L2+R2+X troca a tela e", TEXTO, 1);
    texto(tx, l3, "L2+R2+Círculo troca a suavização.", TEXTO, 1);
    break;
  }
}

void ajustes_desenha(uint32_t *destino, const unsigned char *letras, const Ajustes *a, int frame) {
  px = destino;
  fonte = letras;
  fundo(frame);
  texto_sombra(17 + OY, "AJUSTES", DOURADO, 2);
  olho(W / 2 - 66, 19 + OY);
  olho(W / 2 + 56, 19 + OY);
  fill(W / 2 - 80, 35 + OY, 160, 1, 0xC8962E);
  for (int l = 0; l < 4; l++) linha_ajuste(a, l, frame);
  botao_jogar(a, frame);
  painel(a);
  texto_em(0, W, 184 + OY, "Direcional: escolhe e muda  -  X: joga", BRANCO);
  char linha[64];
  if (a->aviso) {
    texto_em(0, W, 195 + OY, a->aviso, LARANJA);
  } else if (a->segundos >= 0) {
    snprintf(linha, sizeof(linha), "Começa em %d s. Aperte um botão para mudar.", a->segundos);
    texto_em(0, W, 195 + OY, linha, DOURADO);
  } else if (a->situacao) {
    texto_em(0, W, 195 + OY, a->situacao, CINZA);
  }
  texto_em(0, W, 206 + OY, "Yu-Gi-Oh! Forbidden Memories  -  Port para PS2", CINZA);
  if (a->segundos >= 0) {
    snprintf(linha, sizeof(linha), "%d", a->segundos % 100);
    texto(W - 24 - largura(linha, 1), 20 + OY, linha, CINZA, 1);
  }
}

/* ---- teste do modo novo ---- */

void ajustes_confirma(uint32_t *destino, const unsigned char *letras, int video, int segundos, int frame) {
  static const char *const videos[] = {"240p", "480p", "480i"};
  px = destino;
  fonte = letras;
  fundo(frame);
  char linha[64];
  texto_sombra(46 + OY, "A IMAGEM APARECEU?", DOURADO, 2);
  snprintf(linha, sizeof(linha), "Aperte X para ficar com o %s.", videos[video]);
  texto_em(0, W, 80 + OY, linha, BRANCO);
  texto_em(0, W, 92 + OY, "Sem resposta, volta para os ajustes.", TEXTO);
  fill(W / 2 - 80, 65 + OY, 160, 1, 0xC8962E);
  snprintf(linha, sizeof(linha), "%d", segundos < 0 ? 0 : segundos % 100);
  int lw = largura(linha, 5);
  texto((W - lw) / 2 + 1, 116 + OY, linha, 0x000001, 5);
  texto((W - lw) / 2, 115 + OY, linha, DOURADO, 5);
}

/* ---- carregando (depois dos AJUSTES, enquanto o jogo sobe) ---- */

void ajustes_carregando(uint32_t *destino, const unsigned char *letras, int frame) {
  px = destino;
  fonte = letras;
  fundo(frame);
  texto_sombra(64 + OY, "YU-GI-OH!", DOURADO, 2);
  texto_sombra(84 + OY, "FORBIDDEN MEMORIES", DOURADO, 2);
  fill(W / 2 - 100, 103 + OY, 200, 1, 0xC8962E);
  texto_em(0, W, 110 + OY, "Port nativo para PlayStation 2", TEXTO);
  texto_em(0, W, 138 + OY, "Carregando o jogo...", BRANCO);
  texto_em(0, W, 196 + OY, "Retrogames Patos de Minas", CINZA);
}
