#!/bin/bash
# Seria lowrate_check.sh na ustalonej liscie przypadkow (najnizsze osiagalne).
# Uzycie: lowrate_suite.sh ENC FDKDEC  (w katalogu z lr_*.wav, mix48.wav, m1.wav)
ENC=$1; DEC=$2; C=$(dirname "$0")/lowrate_check.sh
while IFS='|' read -r args wav secs; do
  [ -z "$args" ] && continue
  bash "$C" "$ENC" "$DEC" "$args" "$wav" "$secs" < /dev/null
done <<'EOF'
-p 5 -b 8000 -f 2|mix48.wav|10
-p 5 -b 6000 -f 2|mix48.wav|10
-p 5 -b 4000 -f 2|m1.wav|10
-p 5 -b 3000 -f 2|m1.wav|10
-p 5 -b 6000 -f 2|lr_32000_2.wav|4
-p 5 -b 4000 -f 2|lr_24000_1.wav|4
-p 29 -b 8000 -f 2|mix48.wav|10
-p 29 -b 6000 -f 2|mix48.wav|10
-p 29 -b 4000 -f 2|mix48.wav|10
-p 29 -b 3000 -f 2|lr_32000_2.wav|4
-p 39 -L 1 -b 16000 -f 10|mix48.wav|10
-p 39 -L 1 -b 12000 -f 10|lr_48000_2.wav|4
-p 39 -L 1 -b 8000 -f 10|lr_32000_1.wav|4
-p 39 -L 1 -b 8000 -f 10|lr_24000_2.wav|4
-p 39 -b 8000 -f 10|lr_32000_2.wav|4
-p 39 -b 6000 -f 10|lr_24000_1.wav|4
-p 23 -b 8000 -f 10|lr_32000_2.wav|4
-p 23 -b 6000 -f 10|lr_24000_1.wav|4
-p 2 -b 4000 -f 2|mix48.wav|10
-p 2 -b 3000 -f 2|lr_48000_2.wav|4
-p 2 -b 2000 -f 2|lr_24000_1.wav|4
EOF
