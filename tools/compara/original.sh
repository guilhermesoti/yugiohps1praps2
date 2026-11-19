#!/bin/sh
# Abre o jogo ORIGINAL de PS1 no Mednafen, sem filtros, para comparar com o port.
# F9 tira uma foto da tela (vai para ~/.mednafen/snaps). Esc fecha.
# As BIOS do PS1 ficam em ~/.mednafen/firmware (scph5501.bin é a americana).
cd "$(dirname "$0")/../.." || exit 1
CUE="${1:-Yu-Gi-Oh! Forbidden Memories (USA).cue}"
# h_overscan 0: só a área da imagem (sem a borda preta), para alinhar com o port.
exec mednafen -psx.videoip 0 -psx.h_overscan 0 -psx.xscale 3 -psx.yscale 3 -psx.stretch aspect "$CUE"
