# Yu-Gi-Oh! Forbidden Memories — port nativo para PlayStation 2

Port **nativo** de *Yu-Gi-Oh! Forbidden Memories* (PS1, versão americana SLUS-01411) para o
PlayStation 2. Não é um emulador: o código do jogo (decompilado para C) é compilado para o
processador do PS2 (Emotion Engine, MIPS R5900) e a imagem é desenhada pelo chip gráfico do PS2
(o GS). Os dados do jogo (cartas, músicas, modelos 3D e vídeos) são lidos da sua própria imagem
`.bin` do disco, que **não** vem neste repositório.

**Site do projeto:** https://portretrogames.github.io/yugiohps1praps2/ (download, como jogar e o criador do port direto no navegador)

- Como funciona por dentro, mapa das pastas e pontos de atenção para quem for analisar o código:
  **[ARQUITETURA.md](ARQUITETURA.md)**
- Projeto em que este port se baseia: a decompilação + port de PC
  [Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled](https://github.com/Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled)
  (commit `36d499c`). O README original está em
  [docs/projeto-base/README-original.md](docs/projeto-base/README-original.md).

## Como jogar

Baixe o pacote **`YFM_PS2_vX.Y.Z-beta.zip`** da última versão em
[Releases](https://github.com/portretrogames/yugiohps1praps2/releases) e extraia. Ele traz a
pasta `YFM` (ELF, `criar-port.html`, `montar_iso.exe`, tradução, LEIA-ME) e a pasta `ART` (capas do
OPL). **A imagem do jogo não vem no pacote**: use a do seu disco (USA, SLUS-01411).

### Pelo criador (`YFM/criar-port.html` do pacote) — o mais fácil

Abra o `criar-port.html` no navegador (Chrome, Edge ou Firefox), escolha o `.bin` e o `.cue` do seu
disco e o formato. Tudo acontece no navegador, e o jogo não é enviado para lugar nenhum:

- **ISO**: `RGPM_001.01.Yu-Gi-Oh FM PS2.iso` para a pasta `CD` do OPL (com a pasta `ART` do pacote
  na raiz do pendrive, as capas aparecem) ou para abrir no PCSX2. Os saves e as opções ficam no
  cartão de memória (`YFMPS2`). A tradução vai dentro dela (`PTBR.TXT`).
- **Aplicativo**: ZIP com `APPS/YFM_PS2/` (ELF, `YFM.bin`, `title.cfg` e a tradução em
  `languages/pt-br.txt`) para extrair na raiz do pendrive. Aparece na aba Apps do OPL. No Chrome e
  no Edge dá para gravar direto no pendrive.

A página também está solta na página da release, para baixar só ela. Ela não fica no código do
repositório (tem uns 7 MB, com o ELF dentro); o o `build.sh` gera uma nova em `release/criar-port.html` a cada build.

### Pelo ELF (pendrive)

1. No pendrive (FAT32 ou exFAT), crie uma pasta (por exemplo `APPS/YFM`).
2. Copie para ela o conteúdo da pasta `YFM` do pacote (o `YFM_PS2.ELF` e a pasta `languages` com o
   `pt-br.txt`; sem ela o jogo fica em inglês) e a imagem do seu disco renomeada para **`YFM.bin`**
   (o nome original `Yu-Gi-Oh! Forbidden Memories (USA).bin` também funciona).
3. No PS2 abra o ELF pelo OPL (como app) ou pelo wLaunchELF.
4. Os saves ficam em `YFM-SAVE/saves/` dentro da mesma pasta.

No PCSX2: *Sistema → Iniciar ELF*, com o `YFM.bin` na mesma pasta do ELF.

### Pelo `montar_iso.exe` (Windows)

Com a imagem `.bin` do seu disco na pasta `YFM`, dois cliques no `montar_iso.exe` criam
`RGPM_001.01.Yu-Gi-Oh FM PS2.iso`. Ponha a ISO na pasta `CD` do OPL e a pasta `ART` na raiz do
pendrive (capas com o ID `RGPM_001.01`). Pela ISO os saves vão para o memory card (`YFMPS2`). No
Linux: `cc -O2 -o montar_iso tools/ps2/iso/montar_iso.c`.

A ISO tem o seu jogo dentro: não compartilhe.

### Tela AJUSTES (como no port de Super Mario World)

Ao ligar, a tela **AJUSTES** escolhe:

- **Vídeo**: 240p (cabo AV, em qualquer TV) ou 480p (cabo componente ou adaptador HDMI). Um modo novo
  pergunta "A imagem apareceu?" e volta sozinho se ninguém apertar X em 10 s.
- **Posição**: centraliza a imagem na TV (pelo AV muitas TVs mostram a imagem deslocada). Na linha
  Posição, aperte X: aparecem setas nas bordas e as setas do controle movem a imagem, sem perder
  nitidez. X termina. No jogo, **L2+R2 + as setas** fazem o mesmo vendo o jogo.
- **Tela**: 4:3, ou 16:9 (mostra mais do cenário 3D dos lados; a imagem sai espremida para a TV
  em modo 16:9 esticar. O círculo do painel fica redondo com a TV no modo certo).
- **Idioma**: Português (a tradução do port, `languages/pt-br.txt`; é o padrão) ou English (o
  original). Sem o arquivo da tradução, o jogo fica em inglês.
- **Trapaças**: X abre a lista; X liga e desliga, Círculo volta. As prontas: todas as cartas (3 de
  cada), todos os duelistas do Duelo Livre, 999999 StarChips (essas três mexem no save quando ele
  carrega), compras de graça na tela de senha, ver a mão do CPU e duelos com 9999 PV. Mais as de
  código GameShark: de fábrica vêm "StarChips infinitos" e "PV infinitos (você)"; para trocar,
  ponha um `TRAPACAS.TXT` ao lado do ELF (ou na raiz da ISO), uma por linha:
  `Nome = 801D07E0 423F + 801D07E2 000F` (tipos 80, 30, D0, D1, E0 e E1; endereços da RAM do PS1).
  As ligadas ficam em `YFM-TRAPACAS.txt`.

O fundo das telas é feito das artes das cartas, lidas do próprio disco (`WA_MRG.MRG`).

Com os ajustes salvos, a tela começa sozinha em 5 s. No jogo, **L2 + R2 + X** troca 4:3 e 16:9. Tudo fica em `YFM-VISUAL.txt` (no pendrive, ao lado do
ELF; na ISO, no cartão de memória, em `YFMPS2`).

Controle: igual ao do PS1. Ao escolher o alvo de um ataque, **Quadrado** mostra a batalha 3D dos
monstros e **X** faz o ataque rápido.

## Situação

Testado no PCSX2 e num PS2 de verdade (USB exFAT, pelo OPL):

| Parte | Situação |
|---|---|
| Abertura, título, menus, nome, prólogo, montar deck | funciona |
| Duelos (mão, campo 3D, estrelas, fusão, campos, CPU) | funciona, 60 fps |
| Batalha 3D dos monstros (Quadrado) | funciona, 60 fps |
| Vídeos (STR) | 15 fps no PCSX2; no console ainda em ajuste |
| Som | funciona (mixado a 22 kHz) |
| Save / load | funciona: 5 slots em `YFM-SAVE/saves/` |
| Português (BR) | textos, menus, cartas e descrições; nomes das cartas como na dublagem brasileira |

Ao ligar, o port mostra uma abertura da Retrogames e a tela "Apoie o projeto" (START pula a
abertura; X ou START na tela de apoio começa o jogo). As etapas da inicialização vão para o
`YFM-LOG.txt`, gravado ao lado do ELF. Se algo der errado, mande o `YFM-LOG.txt` e uma foto da tela.

## Como compilar

Precisa de Linux (ou WSL), `python3` e o toolchain do PS2 (`ps2dev`: ee-gcc, PS2SDK, gsKit e os
ports libpng, zlib e freetype).

```sh
export PS2DEV=/usr/local/ps2dev     # onde o ps2dev está instalado
./build.sh                          # compila e copia para release/YFM_PS2.ELF
```

O build não precisa da imagem do jogo.

## Créditos

Projeto **gratuito**, feito por fãs. Ele existe graças a:

- **[Yu-Gi-Oh! Forbidden Memories Recompiled](https://github.com/Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled)**
  (Unchiga e contribuidores) e **[memories-decomp](https://github.com/krystalgamer/memories-decomp)**
  (krystalgamer e colaboradores): o jogo decompilado em C, a base de todo o port.
- **[ps2dev](https://github.com/ps2dev/ps2dev)**, **[PS2SDK](https://github.com/ps2dev/ps2sdk)**,
  **[gsKit](https://github.com/ps2dev/gsKit)** e **[ps2sdk-ports](https://github.com/ps2dev/ps2sdk-ports)**:
  as ferramentas e bibliotecas do PS2.
- **[PCSX2](https://github.com/PCSX2/pcsx2)**, **[OPL](https://github.com/ps2homebrew/Open-PS2-Loader)** e
  **[wLaunchELF](https://github.com/ps2homebrew/wLaunchELF)**: para testar e rodar.

Feito por **Guilherme Soti** ([Retrogames Patos de Minas](https://github.com/guilhermesoti)), que
dirigiu o projeto e testou tudo num PS2 de verdade, com a ajuda de inteligências artificiais:
**Claude** (Anthropic) no desenvolvimento, **Gemini** (Google) e **ChatGPT** (OpenAI) nas revisões
técnicas e no conceito do menu de save. O que cada um fez está em **[CREDITOS.md](CREDITOS.md)**.

## Apoie o projeto

O port é e vai continuar gratuito. Se ele te ajudou e você quiser apoiar os próximos (como um port do
Tekken 3), pode doar qualquer valor:

<p align="center">
  <img src="docs/pix-qrcode.png" alt="QR code Pix para doação" width="220"><br>
  <b>Pix (chave aleatória):</b> <code>f19e4d1b-f35c-4541-919b-87a2cb540b9c</code>
</p>

<details>
<summary>Pix copia e cola</summary>

```
00020126580014br.gov.bcb.pix0136f19e4d1b-f35c-4541-919b-87a2cb540b9c5204000053039865802BR5914GUILHERME SOTI6014PATOS DE MINAS62070503***63048E1F
```
</details>

De fora do Brasil? / Outside Brazil? **[ko-fi.com/guilhermesoti](https://ko-fi.com/guilhermesoti)**

<a href="https://ko-fi.com/guilhermesoti"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support me on Ko-fi"></a>

Toda doação é voluntária e não dá acesso a nada extra: o código e as builds continuam livres para
todo mundo. Obrigado por apoiar a cena retro!

## Licença

O código do port de PC e a camada PS2 (`src/ps2/`, `ps2/`, `build.sh`) estão sob a licença MIT (veja
[LICENSE](LICENSE)): mantenha o aviso de copyright e o link para os projetos de origem. O código
decompilado do jogo pertence aos autores do [memories-decomp](https://github.com/krystalgamer/memories-decomp).
*Yu-Gi-Oh!* e *Forbidden Memories* são da Konami; este repositório não tem o jogo nem seus dados, e
você precisa da sua própria cópia. Projeto sem ligação com a Konami.

## Comparar com o jogo original

`tools/compara/` tira a mesma tela do jogo de PS1 (Mednafen) e do port (PCSX2) e mostra as duas lado a lado,
com uma imagem só das diferenças. Veja `tools/compara/LEIA-ME.md`.
