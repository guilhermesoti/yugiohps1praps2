<!-- Texto para colar na página da release. O zip é anexado sozinho pela Action (.github/workflows/release.yml). -->
Port **nativo** de Yu-Gi-Oh! Forbidden Memories (PS1, SLUS-01411) para PlayStation 2, agora **todo em português** e jogável como **ISO no OPL**, com capa.

## Novidades da 0.2.0 beta
- **Tradução completa para português (BR)**: falas, menus, as 721 descrições e os nomes das cartas como na dublagem brasileira (Dragão Branco de Olhos Azuis, Mago Negro, Caveira Invocada...). Ç, ã, é e companhia aparecem certinho.
- **ISO para o OPL**: o `montar_iso.exe` junta o port com a imagem do **seu** disco e cria `RGPM_001.01.Yu-Gi-Oh FM PS2.iso`. Pela ISO os saves vão para o memory card (pasta `YFMPS2`).
- **Capas para o OPL** (pasta `ART`): frente, verso, ícone, logo, fundo e telas, com arte original.
- **Abertura da Retrogames** e tela **Apoie o projeto** com QR code do Pix.
- Corrigido: caixa do FIELD cortada embaixo na TV, letras acentuadas aparecendo como quadrados, linhas nos rostos dos personagens no console, menus da loja desalinhados.

## Como jogar
**Pelo ELF:** extraia a pasta `YFM` no pendrive, coloque a imagem do seu disco como `YFM.bin` e abra o `YFM_PS2.ELF` pelo OPL (como app) ou wLaunchELF.

**Pela ISO:** coloque a imagem `.bin` do seu disco na pasta `YFM`, dê dois cliques no `montar_iso.exe`, ponha a ISO na pasta `CD` do pendrive e a pasta `ART` na raiz.

**A imagem do jogo não vem no pacote**, e a ISO que você montar tem o seu jogo dentro: não compartilhe.

## Problemas conhecidos
- Vídeos ainda em ajuste fino no console.
- O nome escrito no topo da arte da carta e as imagens com texto (menu principal, game over) continuam em inglês.

Achou um bug? Abra uma issue com o `YFM-LOG.txt` (criado ao lado do ELF) e uma foto da tela.

## Apoie
Projeto gratuito e sem fins lucrativos. Se quiser apoiar os próximos ports: Pix e Ko-fi na seção [Apoie o projeto](https://github.com/guilhermesoti/yugiohps1praps2#apoie-o-projeto).

Créditos em [CREDITOS.md](https://github.com/guilhermesoti/yugiohps1praps2/blob/main/CREDITOS.md). Yu-Gi-Oh! é marca de Konami / Kazuki Takahashi; projeto de fã sem ligação com a Konami.
