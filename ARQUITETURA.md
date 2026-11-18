# Arquitetura do port (guia para quem vai ler ou analisar o código)

Este documento explica **como o port funciona**, **onde está cada coisa** e **o que é fácil
interpretar errado** ao ler o código. Foi escrito para pessoas e para outras IAs.

## 1. A ideia em uma frase

O jogo do PS1 foi **decompilado para C** (projeto base, ~1.000 arquivos em `src/game/`). Esse C é
compilado com o `ee-gcc` para o processador do PS2. As partes de hardware do PS1 (GPU, SPU, MDEC,
CD, controle, memory card) são substituídas por implementações em C que usam o hardware do PS2.

**Não** há emulação do processador do PS1 rodando o jogo inteiro, nem recompilação do binário
original em tempo de execução. O executável do PS1 (`SLUS_014.11`) é carregado na memória só como
**dados** (tabelas, textos e ponteiros que o código C lê nos endereços originais).

A única exceção são os pequenos **módulos de coreografia de ataque dos monstros** (código MIPS
dentro de `MODEL.MRG`). A maioria tem versão em C; os que não têm rodam num **interpretador MIPS**
(`src/pc/guest/mips.c`).

## 2. Mapa das pastas

| Pasta | O que é | Quem escreveu |
|---|---|---|
| `src/game/` | O jogo decompilado em C, uma função ou grupo por arquivo (nomes `func_8xxxxxxx` = endereço original) | projeto base |
| `src/psyq/` | Cabeçalhos da biblioteca oficial do PS1 (PsyQ) que o jogo usa | projeto base |
| `src/pc/` | A "plataforma" do port de PC: GPU em software, SPU, CD, saves, menus, GTE, interpretador MIPS etc. **É reaproveitada pelo PS2** | projeto base (+ ajustes `#ifdef MEMORIES_PS2`) |
| `src/overlays/`, `src/native_overlays/` | Módulos que o jogo carrega do disco em tempo de execução (menu, entrada de nome, efeitos de duelo, coreografias de ataque), portados para C | projeto base |
| **`src/ps2/`** | **A camada PS2** (memória, vídeo GS, controle, áudio, USB, falhas). Ver a seção 4 | este port |
| **`ps2/`** | **O build do PS2**: `build_ps2.py` e o `linkfile` do EE | este port |
| `config/` | Tabelas do executável original (endereços de funções e variáveis), lidas pelo build | projeto base |
| `tools/` | Scripts do projeto base; o build do PS2 usa `tools/pc/build_config.py` e `model_gate*.py` | projeto base |
| `notes/`, `docs/` | Anotações da decompilação e documentação do projeto base | projeto base |
| `languages/`, `mods/`, `examples/`, `tests/` | Traduções europeias, sistema de mods e testes do port de PC (não são usados no PS2) | projeto base |
| `release/` | O `YFM_PS2.ELF` pronto | este port |

Tudo o que é específico do PS2 fica em `src/ps2/`, em `ps2/` ou entre `#ifdef MEMORIES_PS2` nos
arquivos de `src/pc/` e `src/game/`. Para ver só o que o port mudou no projeto base:
`grep -rn MEMORIES_PS2 src/`.

## 3. Como o build funciona (`ps2/build_ps2.py`)

1. **Espelha** `src/` em `tmp/ps2/src/`, reescrevendo no caminho:
   - endereços literais do PS1 `0x8001xxxx`–`0x801Fxxxx` → `MEMORIES_RETAIL(...)` (endereço na
     janela de memória do jogo, ver seção 5);
   - toda chamada por ponteiro em assembly (`jalr $25`) → `jal Memories_Icall` (ver
     `src/ps2/thunk.S`).
2. Compila os ~1.270 arquivos do jogo com `-O0` (precisam reproduzir o comportamento exato do
   original) e os ~160 nativos (plataforma) com `-O2`/`-O3`.
3. Gera em `tmp/ps2/gen/`:
   - `guest_symbols.ld`: fixa cada variável global do jogo no endereço original dentro da janela;
   - `stubs.c`: a tabela endereço-do-PS1 → função C (`Memories_FunctionMap`) e os módulos;
   - `guest_branches.S`: funções de módulos chamadas pelo nome.
4. Liga tudo com `ps2/linkfile` (programa em `0x00400000`) → `tmp/ps2/YFM_PS2.ELF`.

## 4. A camada PS2 (`src/ps2/`), arquivo por arquivo

| Arquivo | Função |
|---|---|
| `platform_ps2.c` | `main()`; sobe o IOP e os drivers (USB, controle, áudio); acha o `YFM.bin`; log de boot (`YFM-LOG.txt`); controle (libpad); áudio (audsrv); quadro a quadro (`Platform_Frame`); desenha os menus do port (save) por cima do jogo |
| `guest_ps2.c` | Mapa de memória do jogo (TLB), modo supervisor, carga do executável do PS1 e relocação dos ponteiros dele, `Memories_Resolve` (endereço do PS1 → ponteiro), leitura do disco com read-ahead (`pread`), `fread`/`fwrite` com buffer intermediário, `mmap` (blocos de heap), `rename`/`fsync` para o FAT do USB |
| `gs_gpu.c` / `.h` | **Renderizador**: traduz os comandos da GPU do PS1 (GP0) em primitivas do GS do PS2 |
| `intro_ps2.c` / `intro_qr.h` | Abertura antes do jogo: vinheta da Retrogames (cubo dourado, acorde sintetizado no próprio código), título do port e tela "Apoie o projeto" com o QR code do Pix. Desenhada pela CPU em 640x448 e enviada por `GsGpu_IntroFrame` (gs_gpu.c); os buffers são do heap e liberados no fim (o ELF já usa quase toda a RAM: não use arrays estáticos grandes aqui). No PCSX2 com `host:` só roda se existir `intro.txt` |
| (modo disco) | `platform_ps2.c`: com `argv[0]` em `cdrom0:` (ISO no OPL ou disco gravado) o port não carrega os drivers USB (no OPL o pendrive é do OPL), carrega `cdfs.irx` e lê `cdfs:/YFM.BIN`; idioma em `cdfs:/LANG/PTBR.TXT`; saves em `mc0:/YFMPS2` (`mcman`/`mcserv`). O `fstat` do cdfs devolve tamanho errado: `libds.c` mede por `lseek`. A ISO é montada por `tools/ps2/iso/montar_iso.c` (ID `RGPM_001.01`); capas do OPL em `release/ART` (`tools/ps2/opl_art`). No PCSX2, `host:mctest.txt` testa os saves no memory card |
| `thunk.S` | `Memories_Icall` (chamada por ponteiro: função C ou endereço do PS1), `setjmp`/`longjmp` do jogo (com cópia dos registradores de 64 bits), `VSync` |
| `relocate_ps2.c` | Corrige ponteiros dentro dos módulos que o jogo carrega do disco (e desfaz isso em dados de modelo 3D carregados por cima, ver seção 8) |
| `sampler_ps2.c` | Tratamento de falhas da CPU (tela de erro com o endereço) e emulação de acessos do jogo a endereços físicos do PS1; amostrador de desempenho (só com `host:`) |
| `stubs_ps2.c` | Funções do port de PC que não existem no PS2 (monitor de crash, rede, importação de mods) |
| `irx.S` | Embute no ELF os drivers do IOP (USB, FAT/exFAT, controle, áudio) |
| `force.h` | Incluído em tudo: `MEMORIES_RETAIL()` e constantes da janela de memória |
| `compat/` | Cabeçalhos POSIX mínimos para o código de PC compilar no PS2SDK (ver seção 9) |

## 5. Memória

O PS1 tem 2 MB de RAM em `0x80000000`. No PS2 essa RAM vira uma **janela mapeada pela TLB**:

| Endereço virtual | Físico | Uso |
|---|---|---|
| `0xC0000000`–`0xC01FFFFF` | `0x00200000` | RAM do PS1 (entrada 40 da TLB) |
| `0xC0200000`–`0xC02FFFFF` | `0x00100000` | pilha do interpretador MIPS |
| `0xC0300000` | (mesma página) | scratchpad do PS1 |
| `0x00000000`–`0x0007FFFF` | `0x00200000` | "espelho baixo": ponteiro nulo + deslocamento lê a RAM do jogo, como no PS1 (entrada 47) |
| `0x00400000`… | igual | o programa (ELF) |

- Endereços na janela são **negativos** como `int32`, assim como `0x80xxxxxx` no PS1. Isso mantém
  certos testes de sinal do código original funcionando.
- O jogo roda em **modo supervisor**: acessos perdidos a `0x80xxxxxx` (KSEG0) geram falha, e a
  falha é emulada lendo a janela (`sampler_ps2.c`).
- Num PS2 real, remapear as entradas 13–15 da TLB (espelho de `0x80000`–`0x200000`) **trava o
  console**. Por isso só a entrada 47 é usada; acessos do jogo acima de `0x80000` por ponteiro nulo
  são atendidos pelo tratador de falhas (`sampler_ps2.c`) e cada lugar aparece uma vez no log
  (`retail access served at pc ...`).

## 6. Chamadas por ponteiro (`Memories_Icall`)

O jogo guarda ponteiros de função como endereços do PS1 (`0x8004xxxx`), que depois de relocados
viram `0xC004xxxx` (negativos). `Memories_Icall` (`thunk.S`) faz assim:

- endereço positivo: é uma função C do ELF, chama direto;
- negativo: procura na `Memories_FunctionMap`. Se não achar e o endereço for de um módulo
  carregado (coreografia de ataque), roda no interpretador MIPS.

## 7. Vídeo (`gs_gpu.c`)

- A VRAM do PS1 (1024×512, 16 bits) é uma área de 1 MB da memória do GS (`PSMCT16`, páginas 0–127).
  O PS2 exibe essa área direto.
- Texturas de 15 bits são lidas direto dessa área. Texturas de 4 e 8 bits são copiadas para o
  formato `PSMT4`/`PSMT8` do GS, e a paleta vem **da própria VRAM** (CLUT em modo CSM2). Uma página
  de textura é convertida uma vez e reaproveitada até a CPU mudar aquela região.
- Semitransparência do PS1: duas passadas (texels opacos e texels com o bit de máscara), equação
  `ALPHA` de cada modo do PS1.
- A troca de buffer acontece em `PutDispEnv` (`GsGpu_Show`), como no console. Antes acontecia no
  `VSync`, e a tela piscava mostrando o buffer em desenho.
- Vídeos (STR, 24 bits): o formato do PS1 é igual ao `PSMCT24` do GS; as linhas são copiadas sem
  conversão (páginas 128–167) e exibidas de lá.
- Menus do port (tela de save): desenhados na CPU em resolução dobrada e reduzidos à metade (o texto
  do menu tem no mínimo 16 px), numa janela opaca azul com borda dourada de até 200×130 e escritos como pixels numa **cópia** do buffer exibido; duas cópias se alternam
  (páginas 472–491 e 492–511), então a que está na tela nunca é escrita. A VRAM do jogo nunca é
  alterada.

- **Melhorias visuais** (`GsGpu_SetVisual`): modo 0 original, 1 sem o pontilhado (dithering) do
  PS1, 2 (padrão) sem pontilhado + filtro bilinear. O filtro vale só para polígonos texturizados
  não semitransparentes e sem janela de textura (modelos 3D, campo, cartas girando); sprites,
  texto e menus continuam nítidos. Com o filtro: meio texel a mais no UV, `REGION_CLAMP` no
  retângulo de texels do polígono (sem sangrar do atlas) e teste de alfa `>= 0x20` nas bordas
  recortadas.
- **Tela AJUSTES** (`ajustes.c`, o visual e o fluxo do port de Super Mario World, `formato.c` de lá):
  antes do jogo, desenhada na CPU num quadro 320x240 de 32 bits e mostrada pelo GS
  (`GsGpu_ShowPicture`, páginas 128–167) sempre em 480i. Vídeo 240p/480i (`GsGpu_SetVideo`; o
  480p existe em `display()` mas ficou fora da tela: no PCSX2 sai com o dobro da altura), tela e
  suavização e idioma. Modo novo pede confirmação. `YFM-VISUAL.txt`: quatro dígitos (suavização,
  16:9, vídeo, idioma: 0 português, 1 inglês).
- **Português** (`LANGUAGE_PT_BR` em `src/pc/text/language.c`): a tradução do port,
  `languages/pt-br.txt`, feita do texto espanhol do PAL (mesmos códigos e caixas; layout PAL). As
  letras com acento (Ã, Õ, Ç, Ê...) a fonte do port desenha da letra base com a marca (glyphs.c).
  A tela AJUSTES escolhe por `MEMORIES_LANGUAGE` (6 ou 0); na ISO o arquivo é `PTBR.TXT` na raiz
  (`MEMORIES_LANGUAGE_PACK`), porque a ISO só aceita nomes de 8 letras sem hífen. O pacote é lido
  em blocos até o fim (o `cdrom0:` não responde fseek até o fim). No jogo, L2+R2+X e L2+R2+Círculo (com L2+R2 apertados o jogo não vê o controle).
- **16:9** (`GsGpu_SetWide`, a mesma ideia do `soft_gpu.c` do PC): a área de desenho de tela cheia
  (320 de largura) ganha uma imagem 428 de largura nas páginas 408–463 do GS (os 4 últimos slots do
  cache de texturas, que cai para 15 slots). Cada primitiva é desenhada de novo nela, deslocada 54
  px: polígonos e linhas sem o corte do 4:3, sprites com o corte (menos preenchimentos de largura
  inteira). Cargas, fills e cópias são espelhados no centro por cópia local do GS. Ela é exibida
  com 426 px (MAGH 5). Com um menu do port aberto, a tela volta ao 4:3. A VRAM do jogo não muda.

Mapa da memória do GS (páginas de 8 KB): 0–127 VRAM do PS1, 128–167 vídeo 24 bits / menus,
168–471 cache de texturas (19 slots), 472–491 composição dos menus.

## 7b. Boot por ISO (criador `tools/ps2/creator/`)

- `release/criar-port.html` é gerado pelo `build.sh` (`tools/ps2/make_creator.py`): a página
  (`creator/creator.html`) com o núcleo (`creator/core.js`) e o ELF (gzip + base64) embutidos. O
  núcleo monta a ISO 9660 (SYSTEM.CNF, `RTGM_014.11` = o ELF, `YFM.SIZ`, `YFM.BIN`) ou o ZIP do
  aplicativo, usando o arquivo do jogador como pedaço do Blob. `creator/test_core.cjs` testa no Node.
- ELF iniciado de `cdrom0:` (`iop_start_disc` em `platform_ps2.c`, como o port de Super Mario
  World): **nada de iomanX/fileXio/cdfs nem USB** (no OPL o USB é do próprio OPL, e o IOP dele é
  montado para os módulos da BIOS); XSIO2MAN/XPADMAN/XMCMAN/XMCSERV da BIOS (os do SDK só sem
  eles). O newlib fica no fio: o jogo é `cdrom0:\YFM.BIN;1`. O cdrom0: não responde fstat: o tamanho
  vem do `YFM.SIZ` (`MEMORIES_DISC_SIZE`, `libds.c`). Os arquivos do port ficam em `mc0:/YFMPS2`, e
  não há `YFM-LOG.txt` (gravaria demais no cartão).
- As duas formas de montar a ISO dão boot: a página `criar-port.html` (tradução em `PTBR.TXT` na
  raiz, `YFM.SIZ` com o tamanho) e `tools/ps2/iso/montar_iso` (tradução em `LANG/PTBR.TXT`; o
  tamanho sai do `libds.c`, por lseek ou por busca). Testado no PCSX2: AJUSTES, abertura, jogo em
  português e saves em `mc0:/YFMPS2` com o ícone (o cartão precisa estar formatado). No OPL de
  verdade falta testar.

## 8. Disco, módulos e o bug das batalhas 3D

- `src/pc/sdk/libds.c` emula o drive de CD lendo setores de 2352 bytes do `YFM.bin`. No PS2 cada
  leitura passa pelo IOP e pelo USB, então `pread` (`guest_ps2.c`) usa uma **thread leitora**: ela mantém os 8 blocos de 16 setores (~300 KB) seguintes ao que o jogo está lendo, e enquanto espera o IOP o jogo continua rodando (o console real lia só ~260 KB/s e o vídeo passava 70% do tempo esperando o pendrive).
- Módulos carregados do disco em "bancos" fixos (`0x8013A000`, `0x80146000`, `0x80168000`,
  `0x8017A000`, `0x80180000`) contêm ponteiros do PS1. Eles são reescritos para a janela quando
  chegam (`Ps2_RelocateDelivery`).
- **Cuidado**: os modelos 3D dos monstros são carregados *por cima* desses mesmos bancos. A reescrita
  corrompia vértices (`0x8004xxxx` virava `0xC004xxxx`). Por isso, antes de interpretar um modelo,
  `Ps2_UnrelocateRange` desfaz a reescrita (`src/game/model_load_step.c`).

## 9. Coisas que parecem erradas mas não são (leia antes de "corrigir")

- **`compat/sys/mman.h`, `ucontext.h`, `poll.h`, `fontconfig.h`**: o código de PC inclui esses
  cabeçalhos. No PS2, `mmap` está **implementado** (bloco de heap alinhado, ou arquivo lido para
  memória, em `guest_ps2.c`). `ucontext`/`poll`/`fontconfig` só servem para o código compilar: o PS2
  usa outros caminhos (sem sinais, relógio cooperativo; fonte de bitmap embutida).
- **GTE (coprocessador de geometria do PS1)**: não existe instrução GTE no ELF. O código do jogo
  chama funções C que emulam o GTE (`src/pc/sdk/libgte.c`, `Memories_Gte*`). O interpretador MIPS
  também emula o GTE em C. Nada é executado como COP2 do PS2.
- **`FlushCache(2)` (cache de instruções)**: não é necessário, porque o port não gera código em tempo
  de execução. Todo DMA para o GS passa por `FlushCache(0)` antes (`flush()`/`upload()` em
  `gs_gpu.c`), e os buffers de DMA são `aligned(64)`.
- **Caminhos `cdrom0:\...;1`**: o jogo não abre arquivos do CD pelo sistema de arquivos. Ele lê
  setores da imagem `.bin` e entende o ISO9660 sozinho (projeto base). Os arquivos de verdade abertos
  com `fopen` ficam no USB (`mass:`), com os módulos IRX já carregados no `iop_start()`.
- **`-O0` nos arquivos do jogo**: é proposital. O código decompilado depende do comportamento exato do
  original; otimizar pode mudar resultados.
- **Literais `0x80...` reescritos só na cópia (`tmp/ps2/src`)**: os fontes em `src/game/` mostram os
  endereços originais do PS1, e o build converte na cópia.

## 10. Diagnóstico

- `YFM-LOG.txt` (ao lado do ELF): etapas do boot (fechado e reaberto a cada etapa, para sobreviver
  a um travamento); depois dos primeiros 120 quadros ele passa a ser gravado em blocos de 16 KB, sem
  tocar no pendrive a cada segundo (isso engasgava os vídeos).
- Tela de erro: `Exception N at <pc> (address <endereço>, ra ...)`. Para achar a função:
  `mips64r5900el-ps2-elf-addr2line -f -e tmp/ps2/YFM_PS2.ELF <pc>`. O endereço só vale para o
  mesmo build.
- No PCSX2 com o ELF iniciado de `host:`: entrada por arquivo (`input.txt`, `cmd.txt`), dumps de
  VRAM/RAM e amostrador de desempenho (`platform_ps2.c`).

- No PCSX2 com `host:`: `host:env.txt` (linhas `CHAVE=VALOR`, por exemplo `MEMORIES_MODE_AT`) e,
  no `cmd.txt`, o passo `1:FFFC` grava a imagem exibida em `host:shot.bin`
  (`tools/ps2_shot2png.py shot.bin foto.png` converte).
- **L1 + R1 + L2 + R2 + SELECT** no console grava `YFM-GSVRAM.bin` e `YFM-CPUVRAM.bin` (a VRAM do
  GS e a da CPU, 1024×512 16 bits) ao lado do ELF: para comparar o que o console desenhou com o
  emulador quando uma imagem sai diferente.

## 11. Pendências conhecidas

- Vídeos no console real: ainda podem engasgar (decodificação MDEC em software no EE + leitura do
  USB).
- Áudio a 22 kHz (metade da taxa original) para caber no tempo do EE.
- Tradução PT-BR (`languages/pt-br.txt`, idioma 6 = `LANGUAGE_PT_BR`, padrão no PS2): feita a
  partir do `es.txt` (mesmos códigos e caixas do PAL), descrições refeitas em 20 colunas e até
  8 linhas. Falta: o nome escrito no topo da arte da carta continua em inglês no PS2 (no PC o
  `soft_gpu` redesenha a placa; o GS não), e as imagens com texto (menu principal, game over).
  Ferramentas usadas: `tools/ps2/pt_br/` (montagem e conferência de tamanho das linhas).
