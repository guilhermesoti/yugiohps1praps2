#!/bin/sh
# Junta a foto do jogo original (Mednafen, F9) com a do port (PCSX2, F8) e mostra as diferenças.
#
#   sh tools/compara/lado_a_lado.sh                    # usa a última foto de cada emulador
#   sh tools/compara/lado_a_lado.sh original.png port.png
#
# Gera em tools/compara/saida/:
#   AAAAMMDD-HHMMSS-lado.png  original à esquerda, port à direita (640x480 cada)
#   AAAAMMDD-HHMMSS-dif.png   só o que muda entre as duas (preto = igual)
ORIG="$1"
PORT="$2"
[ -n "$ORIG" ] || ORIG=$(ls -t "$HOME"/.mednafen/snaps/*.png 2>/dev/null | head -1)
[ -n "$PORT" ] || PORT=$(ls -t "$HOME"/.config/PCSX2/snaps/*.png 2>/dev/null | head -1)
if [ ! -f "$ORIG" ] || [ ! -f "$PORT" ]; then
    echo "Falta foto: original='$ORIG' port='$PORT'"
    echo "Tire uma com F9 no Mednafen (tools/compara/original.sh) e outra com F8 no PCSX2."
    exit 1
fi
SAIDA="$(dirname "$0")/saida"
mkdir -p "$SAIDA"
NOME="$SAIDA/$(date +%Y%m%d-%H%M%S)"
# O Mednafen deixa faixas pretas dos lados (a borda da TV); tira só elas, para alinhar.
TMP=$(mktemp --suffix=.png)
trap 'rm -f "$TMP"' EXIT
if command -v magick >/dev/null 2>&1; then IM=magick; else IM=convert; fi
$IM "$ORIG" -fuzz 6% -define trim:edges=west,east -trim +repage "$TMP" 2>/dev/null && ORIG_USADA="$TMP" || ORIG_USADA="$ORIG"
# As duas no mesmo tamanho (sem suavizar, para não esconder diferença de pixel).
F="scale=640:480:flags=neighbor,format=rgb24"
ffmpeg -loglevel error -y -i "$ORIG_USADA" -i "$PORT" \
    -filter_complex "[0:v]$F[a];[1:v]$F[b];[a][b]hstack=inputs=2" -frames:v 1 -update 1 "$NOME-lado.png" &&
ffmpeg -loglevel error -y -i "$ORIG_USADA" -i "$PORT" \
    -filter_complex "[0:v]$F[a];[1:v]$F[b];[a][b]blend=all_mode=difference,lutrgb=r=min(val*4\,255):g=min(val*4\,255):b=min(val*4\,255)" \
    -frames:v 1 -update 1 "$NOME-dif.png" &&
echo "Original: $ORIG" && echo "Port:     $PORT" &&
echo "Pronto:   $NOME-lado.png e $NOME-dif.png"
