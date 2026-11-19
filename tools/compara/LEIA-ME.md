# Comparar o port com o jogo original

Serve para achar diferença de imagem (cor, posição, texto) entre o port e o jogo de PS1.

1. **Original:** `sh tools/compara/original.sh` abre o jogo no Mednafen, sem filtros.
   Na tela que quer comparar, aperte **F9** (a foto vai para `~/.mednafen/snaps`).
   As BIOS do PS1 ficam em `~/.mednafen/firmware` (`scph5501.bin` é a americana).
2. **Port:** no PCSX2, na mesma tela, aperte **F8** (a foto vai para `~/.config/PCSX2/snaps`).
   Para comparar com o original, use a suavização "original" e 4:3 na tela AJUSTES.
3. `sh tools/compara/lado_a_lado.sh` junta as duas últimas fotos (ou passe os dois arquivos).

Em `tools/compara/saida/` saem duas imagens:
- `...-lado.png`: original à esquerda, port à direita;
- `...-dif.png`: só o que muda (preto = igual). Riscos verticais bem fracos são da diferença de
  escala entre os emuladores; o que importa são as manchas fortes.

Precisa de `mednafen`, `ffmpeg` e ImageMagick (`sudo apt install mednafen ffmpeg imagemagick`).
