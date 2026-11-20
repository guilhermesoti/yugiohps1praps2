/* Telas AJUSTES e "A imagem apareceu?" (ajustes.c), no visual do port de
 * Super Mario World. Quadro de 320 x 240 pontos, 0xRRGGBB. */
#ifndef MEMORIES_PS2_AJUSTES_H
#define MEMORIES_PS2_AJUSTES_H
#include <stdint.h>
#include "trapacas.h"

#define AJUSTES_W 320
#define AJUSTES_H 240

/* Modos de vídeo, na ordem do port do Mario (o arquivo guarda o número). */
enum { VIDEO_240P = 0, VIDEO_480P = 1, VIDEO_480I = 2 };
/* A Suavização saiu da tela em 20/11/2026 (pedido do usuário): o jogo fica como no PS1. */
/* O ajuste de tamanho (altura/largura) saiu em 20/11/2026: em 240p diminuir a
 * altura embaça ou corta linhas. Fica só a posição, para centralizar na TV. */
enum { AJUSTE_VIDEO, AJUSTE_TELA, AJUSTE_IDIOMA, AJUSTE_POSICAO, AJUSTE_TRAPACAS, AJUSTE_JOGAR, AJUSTE_LINHAS };
/* Tamanho da imagem na TV, largura e altura separadas: de 100% a 80%, de 2
 * em 2 (no arquivo, uma letra cada: 'a' = 100%, 'k' = 80%). */
#define AJUSTE_TAM_MIN 80
#define AJUSTE_TAM_PASSO 2
/* Posição: de 2 em 2 pontos/linhas, de -16 a 16 (no arquivo, uma letra cada:
 * 'A' = -16, 'I' = 0, 'Q' = 16). */
#define AJUSTE_POS_PASSO 2
#define AJUSTE_POS_MAX 16

typedef struct {
    int linha;            /* AJUSTE_* marcada */
    int video, tela, suave;
    int idioma;           /* 0 português, 1 English (o original) */
    int sem_traducao;     /* falta o languages/pt-br.txt: só o inglês */
    int segundos;         /* contagem para começar sozinho; -1 sem contagem */
    const char *aviso;    /* em laranja, por exemplo "A TV não mostrou o 480p" */
    const char *situacao; /* em cinza: onde ficam os saves */
    int largura, altura;  /* tamanho na TV, % */
    int ajustando;        /* no modo de ajuste (setas nas bordas) */
    int movendo;          /* no ajuste: as setas movem a imagem (Quadrado alterna) */
    int pos_x, pos_y;     /* posição na TV */
    int trapacas;         /* quantas ligadas */
} Ajustes;

/* O fundo temático: artes de cartas lidas do disco do jogador (n artes de
 * ARTE_W x ARTE_H, 8 bits por ponto, e as paletas de 256 cores do PS1, 15
 * bits). Com n = 0 volta o fundo azul com estrelas. Os buffers são do
 * chamador e precisam durar enquanto as telas forem desenhadas. */
#define ARTE_W 102
#define ARTE_H 96
void ajustes_artes(const uint8_t *indices, const uint16_t *paletas, int n);

void ajustes_desenha(uint32_t *destino, const unsigned char *letras, const Ajustes *a, int frame);
/* A lista das trapaças: `cursor` a marcada, `codigos` quantas vieram do
 * TRAPACAS.TXT (-1: o arquivo não foi achado). */
void ajustes_trapacas(uint32_t *destino, const unsigned char *letras, const Trapacas *t, int cursor, int codigos,
                      int frame);
void ajustes_confirma(uint32_t *destino, const unsigned char *letras, int video, int segundos, int frame);
void ajustes_carregando(uint32_t *destino, const unsigned char *letras, int frame);
#endif
