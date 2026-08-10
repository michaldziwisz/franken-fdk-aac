#!/bin/bash
# Zbieranie dumpu OPD dla wszystkich probek diagnostycznych.
set -u
cd /mnt/d/projekty/aacfdk/tools/opd
ENC="$HOME/aacfdk_native/front/fdkaac"
mkdir -p dumps
for f in *.wav; do
  base="${f%.wav}"
  OPD_DUMP=1 "$ENC" -p 29 -b 48000 --ps-ipd 1 --ps-opd 1 \
      -o "dumps/$base.m4a" "$f" 2> "dumps/$base.err"
  n=$(grep -c OPDDUMP "dumps/$base.err" || true)
  sz=$(stat -c%s "dumps/$base.m4a" 2>/dev/null || echo 0)
  printf '%-32s dump=%-7s m4a=%s B\n' "$base" "$n" "$sz"
done
