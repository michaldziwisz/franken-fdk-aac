#!/bin/bash
# Regeneracja dokumentacji po zmianach w zrodlowych .md (wg skilla).
# README.md jest PLIKIEM POCHODNYM: README.en-part.md + separator + README.pl.md.
# gen_docs.sh PRZYJMUJE ARGUMENTY: <in.md> <lang> <title> <outbase>
# Trzy OSOBNE wywolania, kazde z wlasnym tytulem. unset LD_LIBRARY_PATH przed.
set -eu
cd /mnt/d/projekty/aacfdk
unset LD_LIBRARY_PATH || true

# 1) zloz README.md ze zrodel (NIE edytowac go recznie)
{
  cat README.en-part.md
  printf '\n\n# PL Wersja polska\n\n'
  cat README.pl.md
} > README.md
echo "README.md zlozony: $(wc -l < README.md) linii"

bash gen_docs.sh README.md    en "Franken FDK AAC — README (EN/PL)" README
bash gen_docs.sh MANUAL.en.md en "Franken FDK AAC — Manual (EN)"   MANUAL.en
bash gen_docs.sh MANUAL.md    pl "Franken FDK AAC — Manual (PL)"   MANUAL

echo
echo "=== weryfikacja PDF: Title niepusty ORAZ MarkInfo Marked=true ==="
python3 - <<'PY'
import fitz
for p in ("README.pdf", "MANUAL.en.pdf", "MANUAL.pdf"):
    try:
        d = fitz.open(p)
        t = d.metadata.get("title") or ""
        raw = d.xref_object(d.pdf_catalog())
        marked = "Marked" in raw and "true" in raw.split("Marked")[1][:20]
        print("  %-16s Title=%-40r Marked=%s stron=%d"
              % (p, t[:38], marked, d.page_count))
    except Exception as e:
        print("  %-16s BLAD: %s" % (p, e))
PY
