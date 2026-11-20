/* Trapaças: a lista (as prontas e as do TRAPACAS.TXT) e os códigos
 * GameShark (trapacas.h). Sem nada do PS2: testado no PC
 * (tests/ps2/trapacas_teste.c). */
#include "trapacas.h"
#include <string.h>

/* Endereços da decompilação: StarChips em gLibrary_dwStarchips (0x801D07E0,
 * 32 bits) e os PV do jogador em D_800E9FF0[0].life_points (+0x14). */
const char trapacas_padrao[] =
    "# Trapaças de fábrica (crie um TRAPACAS.TXT para trocar)\n"
    "StarChips infinitos = 801D07E0 423F + 801D07E2 000F\n"
    "PV infinitos (você) = 800EA004 270F\n";

static const char *const prontas[TRAPACA_PRONTAS] = {
    [TRAPACA_CARTAS] = "Todas as cartas (3 de cada)",
    [TRAPACA_DUELISTAS] = "Todos os duelistas liberados",
    [TRAPACA_STARCHIPS] = "StarChips 999999",
    [TRAPACA_COMPRAS] = "Compras de graça (Senha)",
    [TRAPACA_MAO_CPU] = "Ver a mão do CPU",
    [TRAPACA_PV_9999] = "Duelos com 9999 PV",
};

static int hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int espaco(int c) { return c == ' ' || c == '\t' || c == '\r'; }

/* Copia o nome sem os espaços das pontas, até TRAPACA_NOME letras; fica o
 * que a fonte das telas mostra (ASCII e as letras com acento, 0xC3 xx). */
static void copia_nome(char *dst, const char *s, size_t n)
{
    size_t o = 0, i = 0;
    int letras = 0;
    while (n && espaco((unsigned char)*s)) ++s, --n;
    while (n && espaco((unsigned char)s[n - 1])) --n;
    while (i < n && letras < TRAPACA_NOME) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x20 && c < 0x7F) {
            dst[o++] = (char)c, ++i;
        } else if (c == 0xC3 && i + 1 < n && ((unsigned char)s[i + 1] & 0xC0) == 0x80) {
            dst[o++] = (char)c, dst[o++] = s[i + 1], i += 2;
        } else {
            dst[o++] = '?', ++i;
            while (i < n && ((unsigned char)s[i] & 0xC0) == 0x80) ++i;
        }
        ++letras;
    }
    while (o && dst[o - 1] == ' ') --o;
    dst[o] = '\0';
}

/* Um código em s[0..n): 8 algarismos, um separador opcional (espaço ou
 * ':'), 4 algarismos. */
static int codigo(const char *s, size_t n, TrapacaCodigo *c)
{
    char d[12];
    size_t k = 0, i;
    uint32_t a = 0;
    uint16_t v = 0;
    unsigned tipo;
    for (i = 0; i < n; ++i) {
        if ((s[i] == ' ' || s[i] == ':' || s[i] == '\t') && k == 8) continue;
        if (hex((unsigned char)s[i]) < 0 || k == 12) return 0;
        d[k++] = s[i];
    }
    if (k != 12) return 0;
    for (i = 0; i < 8; ++i) a = a << 4 | (uint32_t)hex(d[i]);
    for (i = 8; i < 12; ++i) v = (uint16_t)(v << 4 | (uint16_t)hex(d[i]));
    tipo = a >> 24;
    if (tipo != 0x80 && tipo != 0x30 && tipo != 0xD0 && tipo != 0xD1 && tipo != 0xE0 && tipo != 0xE1) return 0;
    if ((a & 0xFFFFFF) >= 0x200000) return 0;                       /* fora dos 2 MB */
    if ((tipo == 0x80 || tipo == 0xD0 || tipo == 0xD1) && (a & 1)) return 0; /* 16 bits: endereço par */
    c->endereco = a, c->valor = v;
    return 1;
}

void trapacas_inicia(Trapacas *t)
{
    int i;
    memset(t, 0, sizeof(*t));
    for (i = 0; i < TRAPACA_PRONTAS; i++) copia_nome(t->t[i].nome, prontas[i], strlen(prontas[i]));
    t->n = TRAPACA_PRONTAS;
}

int trapacas_le(Trapacas *t, const char *texto, size_t n)
{
    size_t i = 0;
    int novas = 0, k;
    while (i < n) {
        size_t fim = i, igual;
        while (fim < n && texto[fim] != '\n') ++fim;
        for (igual = i; igual < fim && texto[igual] != '='; ++igual) {}
        if (igual < fim && texto[i] != '#' && t->n < TRAPACA_MAX) {
            Trapaca *tr = &t->t[t->n];
            size_t c = igual + 1;
            int ok = 1;
            memset(tr, 0, sizeof(*tr));
            copia_nome(tr->nome, texto + i, igual - i);
            while (c < fim && ok) {
                size_t e = c, ini, fi;
                while (e < fim && texto[e] != '+' && texto[e] != ',') ++e;
                ini = c, fi = e;
                while (ini < fi && espaco((unsigned char)texto[ini])) ++ini;
                while (fi > ini && espaco((unsigned char)texto[fi - 1])) --fi;
                if (fi > ini) {
                    if (tr->codigos == TRAPACA_CODIGOS || !codigo(texto + ini, fi - ini, &tr->codigo[tr->codigos])) ok = 0;
                    else tr->codigos++;
                }
                c = e + 1;
            }
            /* Uma linha com um código ruim sai inteira: meia trapaça pode
             * escrever na RAM o que o resto devia completar. */
            /* Nome repetido sai também: o que fica ligado é salvo pelo nome. */
            for (k = 0; ok && k < t->n; k++)
                if (!strcmp(t->t[k].nome, tr->nome)) ok = 0;
            if (ok && tr->codigos && tr->nome[0]) t->n++, novas++;
        }
        i = fim + 1;
    }
    return novas;
}

int trapacas_liga_salvas(Trapacas *t, const char *texto, size_t n)
{
    size_t i = 0;
    int ligou = 0, k;
    while (i < n) {
        size_t fim = i;
        char nome[TRAPACA_NOME * 2 + 1];
        while (fim < n && texto[fim] != '\n') ++fim;
        copia_nome(nome, texto + i, fim - i);
        for (k = 0; nome[0] && k < t->n; k++)
            if (!t->t[k].ligada && !strcmp(t->t[k].nome, nome)) {
                t->t[k].ligada = 1, ligou++;
                break;
            }
        i = fim + 1;
    }
    return ligou;
}

size_t trapacas_texto_ligadas(const Trapacas *t, char *texto, size_t tamanho)
{
    size_t o = 0;
    int k;
    if (!tamanho) return 0;
    for (k = 0; k < t->n; k++) {
        size_t l = strlen(t->t[k].nome);
        if (!t->t[k].ligada || o + l + 2 > tamanho) continue;
        memcpy(texto + o, t->t[k].nome, l);
        o += l;
        texto[o++] = '\n';
    }
    texto[o] = '\0';
    return o;
}

int trapacas_ligadas(const Trapacas *t)
{
    int k, n = 0;
    for (k = 0; k < t->n; k++) n += t->t[k].ligada != 0;
    return n;
}

void trapacas_aplica_codigos(const Trapacas *t, uint8_t *ram)
{
    int k, i;
    for (k = TRAPACA_PRONTAS; k < t->n; k++) {
        const Trapaca *tr = &t->t[k];
        int pula = 0;
        if (!tr->ligada) continue;
        for (i = 0; i < tr->codigos; i++) {
            uint32_t a = tr->codigo[i].endereco & 0x1FFFFF;
            uint16_t v = tr->codigo[i].valor, atual16;
            unsigned tipo = tr->codigo[i].endereco >> 24;
            if (pula) {
                pula = 0;
                continue;
            }
            switch (tipo) {
            case 0x80: ram[a] = (uint8_t)v, ram[a + 1] = (uint8_t)(v >> 8); break;
            case 0x30: ram[a] = (uint8_t)v; break;
            case 0xD0: case 0xD1:
                atual16 = (uint16_t)(ram[a] | ram[a + 1] << 8);
                pula = tipo == 0xD0 ? atual16 != v : atual16 == v;
                break;
            case 0xE0: case 0xE1:
                pula = tipo == 0xE0 ? ram[a] != (uint8_t)v : ram[a] == (uint8_t)v;
                break;
            }
        }
    }
}
