#!/usr/bin/env bash
# gen_docs.sh <input.md> <lang> <title> <outbase>
# Generuje <outbase>.pdf (dostepny PDF/UA), <outbase>.rtf, <outbase>.html
set -u
IN="$1"; LANG="$2"; TITLE="$3"; OUT="$4"
DIR=/mnt/d/projekty/aacfdk
cd "$DIR"

# HTML: standalone, dostepny (lang, semantyczne naglowki, TOC)
pandoc "$IN" -o "$OUT.html" --from gfm --standalone --toc --toc-depth=2 \
  --metadata lang="$LANG" --metadata title="$TITLE" 2>/dev/null
echo "  $OUT.html: $(stat -c%s "$OUT.html" 2>/dev/null) B"

# DOCX posredni -> PDF/UA + RTF przez LibreOffice
pandoc "$IN" -o "$OUT.docx" --from gfm --metadata lang="$LANG" 2>/dev/null
soffice -env:UserInstallation="file:///tmp/lo_$$_p" --headless \
  --convert-to 'pdf:writer_pdf_Export:{"UseTaggedPDF":{"type":"boolean","value":"true"},"PDFUACompliance":{"type":"boolean","value":"true"},"SelectPdfVersion":{"type":"long","value":"0"}}' \
  --outdir "$DIR" "$OUT.docx" >/dev/null 2>&1
# nazwa pdf = basename docx; zmien jesli trzeba
BASE=$(basename "$OUT.docx" .docx)
[ "$BASE.pdf" != "$OUT.pdf" ] && [ -f "$BASE.pdf" ] && mv "$BASE.pdf" "$OUT.pdf"
# Ustaw tytul PDF (dostepnosc PDF/UA - czytnik ekranu oglasza tytul dokumentu).
# soffice/pandoc nie przenosza --metadata title do PDF Title, wiec robimy to przez fitz.
python3 - "$OUT.pdf" "$TITLE" <<'PY' 2>/dev/null || true
import sys, fitz
d = fitz.open(sys.argv[1]); m = d.metadata; m["title"] = sys.argv[2]
d.set_metadata(m); d.saveIncr(); d.close()
PY
echo "  $OUT.pdf: $(stat -c%s "$OUT.pdf" 2>/dev/null) B"

# RTF: pandoc bezposrednio (bez soffice - unika blokady przy sekwencyjnych konwersjach)
pandoc "$IN" -o "$OUT.rtf" --from gfm --standalone 2>/dev/null
echo "  $OUT.rtf: $(stat -c%s "$OUT.rtf" 2>/dev/null) B"
