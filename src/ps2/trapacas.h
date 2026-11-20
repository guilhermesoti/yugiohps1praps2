/* Trapaças do port no PS2 (como no port de Super Mario World, trapacas.c de
 * lá): ligadas na tela AJUSTES, linha Trapaças.
 *
 * Duas fontes:
 *  - as prontas, que usam o código de trapaças do projeto base
 *    (pc/debug/cheats.c): dar todas as cartas, liberar os duelistas do Duelo
 *    Livre, StarChips, PV iniciais, compras de graça, ver a mão do CPU;
 *  - códigos GameShark do TRAPACAS.TXT, aplicados na RAM do PS1 a cada
 *    quadro. A memória do jogo fica nos mesmos endereços do console (a
 *    janela do convidado, memory_map.h), então um código de RAM funciona
 *    igual.
 *
 * TRAPACAS.TXT (UTF-8, uma trapaça por linha):
 *     # comentário
 *     Nome da trapaça = 801D07E0 423F + 801D07E2 000F
 * Cada código: 8 algarismos hexadecimais do endereço e 4 do valor (com
 * espaço, ':' ou nada no meio), separados por '+' ou ','. Tipos aceitos:
 *   80aaaaaa vvvv  escreve 16 bits     30aaaaaa 00vv  escreve 8 bits
 *   D0aaaaaa vvvv  só se os 16 bits forem iguais a vvvv (vale para o próximo)
 *   D1aaaaaa vvvv  só se forem diferentes        E0/E1: o mesmo com 8 bits
 * Endereços fora dos 2 MB da RAM são recusados (a linha inteira sai). */
#ifndef MEMORIES_PS2_TRAPACAS_H
#define MEMORIES_PS2_TRAPACAS_H
#include <stddef.h>
#include <stdint.h>

enum { TRAPACA_MAX = 40, TRAPACA_NOME = 32, TRAPACA_CODIGOS = 16, TRAPACAS_BYTES = 16384 };

/* As prontas (as primeiras da lista). */
enum {
    TRAPACA_CARTAS,     /* 3 de cada carta no baú, ao carregar um save */
    TRAPACA_DUELISTAS,  /* todos os duelistas do Duelo Livre, ao carregar */
    TRAPACA_STARCHIPS,  /* 999999 StarChips, ao carregar */
    TRAPACA_COMPRAS,    /* comprar na tela de senha sem gastar */
    TRAPACA_MAO_CPU,    /* ver a mão do CPU no duelo */
    TRAPACA_PV_9999,    /* os dois lados começam o duelo com 9999 PV */
    TRAPACA_PRONTAS
};

typedef struct {
    uint32_t endereco; /* o do código, com o tipo no byte de cima */
    uint16_t valor;
} TrapacaCodigo;

typedef struct {
    char nome[TRAPACA_NOME * 2 + 1]; /* até TRAPACA_NOME letras (UTF-8) */
    TrapacaCodigo codigo[TRAPACA_CODIGOS];
    int codigos; /* 0 nas prontas */
    int ligada;
} Trapaca;

typedef struct {
    Trapaca t[TRAPACA_MAX];
    int n;
} Trapacas;

/* O TRAPACAS.TXT de fábrica, usado quando não há um no disco ou na pasta. */
extern const char trapacas_padrao[];

/* As prontas, desligadas. */
void trapacas_inicia(Trapacas *t);
/* Acrescenta as do texto do TRAPACAS.TXT (n bytes). Volta quantas entraram. */
int trapacas_le(Trapacas *t, const char *texto, size_t n);
/* Liga as que o texto (uma por linha, o nome) lista; volta quantas. */
int trapacas_liga_salvas(Trapacas *t, const char *texto, size_t n);
/* O texto das ligadas, um nome por linha. Volta o tamanho. */
size_t trapacas_texto_ligadas(const Trapacas *t, char *texto, size_t tamanho);
int trapacas_ligadas(const Trapacas *t);

/* Os códigos GameShark das ligadas na RAM do PS1 (`ram`: o começo dos 2 MB). */
void trapacas_aplica_codigos(const Trapacas *t, uint8_t *ram);
#endif
