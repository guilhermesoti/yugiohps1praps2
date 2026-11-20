/* Telas AJUSTES e "A imagem apareceu?" (ajustes.c), no visual do port de
 * Super Mario World. Quadro de 320 x 240 pontos, 0xRRGGBB. */
#ifndef MEMORIES_PS2_AJUSTES_H
#define MEMORIES_PS2_AJUSTES_H
#include <stdint.h>

#define AJUSTES_W 320
#define AJUSTES_H 240

/* Modos de vídeo, na ordem do port do Mario (o arquivo guarda o número). */
enum { VIDEO_240P = 0, VIDEO_480P = 1, VIDEO_480I = 2 };
enum { AJUSTE_VIDEO, AJUSTE_TELA, AJUSTE_SUAVE, AJUSTE_IDIOMA, AJUSTE_TAMANHO, AJUSTE_JOGAR, AJUSTE_LINHAS };
/* Tamanho da imagem na TV, largura e altura separadas: de 100% a 80%, de 2
 * em 2 (no arquivo, uma letra cada: 'a' = 100%, 'k' = 80%). */
#define AJUSTE_TAM_MIN 80
#define AJUSTE_TAM_PASSO 2

typedef struct {
    int linha;            /* AJUSTE_* marcada */
    int video, tela, suave;
    int idioma;           /* 0 português, 1 English (o original) */
    int sem_traducao;     /* falta o languages/pt-br.txt: só o inglês */
    int segundos;         /* contagem para começar sozinho; -1 sem contagem */
    const char *aviso;    /* em laranja, por exemplo "A TV não mostrou o 480p" */
    const char *situacao; /* em cinza: onde ficam os saves */
    int largura, altura;  /* tamanho na TV, % */
    int ajustando;        /* no modo de ajuste do tamanho (setas nas bordas) */
} Ajustes;

void ajustes_desenha(uint32_t *destino, const unsigned char *letras, const Ajustes *a, int frame);
void ajustes_confirma(uint32_t *destino, const unsigned char *letras, int video, int segundos, int frame);
void ajustes_carregando(uint32_t *destino, const unsigned char *letras, int frame);
#endif
