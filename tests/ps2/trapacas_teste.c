/* Teste do leitor de trapaças (src/ps2/trapacas.c) no PC:
 *   gcc -std=c99 -Wall -fsanitize=address,undefined -o /tmp/t tests/ps2/trapacas_teste.c src/ps2/trapacas.c && /tmp/t */
#include "../../src/ps2/trapacas.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { printf("FALHOU linha %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main(void) {
  static Trapacas t; static uint8_t ram[0x200000]; char buf[4096];
  const char *txt =
    "# comentario\n"
    "StarChips (código) = 801D07E0 423F + 801D07E2 000F\n"
    "Byte = 300EA004 0063\n"
    "Ruim fora = 80300000 0001\n"
    "Ruim impar = 801D07E1 0001\n"
    "Ruim tipo = 121D07E0 0001\n"
    "Condicional = D01D0000 0005 + 801D0010 1234 + 801D0012 5678\n"
    "Junto:sem espaco = 801D0020:ABCD,301D0022 00EF\r\n"
    "sem igual nenhum\n"
    "Byte = 801D0030 0001\n";
  trapacas_inicia(&t);
  CHECK(t.n == TRAPACA_PRONTAS);
  CHECK(!strcmp(t.t[TRAPACA_COMPRAS].nome, "Compras de graça (Senha)"));
  int n = trapacas_le(&t, txt, strlen(txt));
  CHECK(n == 4);
  CHECK(!strcmp(t.t[TRAPACA_PRONTAS].nome, "StarChips (código)"));
  CHECK(t.t[TRAPACA_PRONTAS].codigos == 2);
  for (int k = 0; k < t.n; k++) t.t[k].ligada = 1;
  ram[0x1D0000] = 5;
  trapacas_aplica_codigos(&t, ram);
  CHECK(ram[0x1D07E0] == 0x3F && ram[0x1D07E1] == 0x42 && ram[0x1D07E2] == 0x0F && ram[0x1D07E3] == 0);
  CHECK(ram[0x0EA004] == 0x63);
  CHECK(ram[0x1D0010] == 0x34 && ram[0x1D0011] == 0x12); /* D0 verdadeiro: aplica o próximo */
  CHECK(ram[0x1D0012] == 0x78);                          /* e o seguinte sempre */
  CHECK(ram[0x1D0020] == 0xCD && ram[0x1D0021] == 0xAB && ram[0x1D0022] == 0xEF);
  memset(ram, 0, sizeof(ram)); ram[0x1D0000] = 4;        /* D0 falso: pula só o próximo */
  trapacas_aplica_codigos(&t, ram);
  CHECK(ram[0x1D0010] == 0 && ram[0x1D0012] == 0x78);
  /* salvar e ligar de novo */
  t.t[1].ligada = 0;
  size_t l = trapacas_texto_ligadas(&t, buf, sizeof(buf));
  CHECK(l > 0 && !strstr(buf, "Todos os duelistas"));
  static Trapacas u; trapacas_inicia(&u); trapacas_le(&u, txt, strlen(txt));
  CHECK(trapacas_liga_salvas(&u, buf, l) == t.n - 1);
  CHECK(trapacas_ligadas(&u) == t.n - 1 && !u.t[1].ligada && u.t[TRAPACA_PRONTAS].ligada);
  /* lista cheia */
  static Trapacas w; trapacas_inicia(&w);
  char grande[20000] = ""; for (int i = 0; i < 60; i++) { char l2[64]; sprintf(l2, "T%d = 800EA004 270F\n", i); strcat(grande, l2); }
  trapacas_le(&w, grande, strlen(grande));
  CHECK(w.n == TRAPACA_MAX);
  static Trapacas p; trapacas_inicia(&p);
  CHECK(trapacas_le(&p, trapacas_padrao, strlen(trapacas_padrao)) == 2);
  printf("ok: %d trapaças do texto, lista com %d\n", n, t.n);
  return 0;
}
