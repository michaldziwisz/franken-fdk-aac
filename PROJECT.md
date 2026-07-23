# Franken FDK AAC

A "Frankenstein" build of the Fraunhofer FDK AAC encoder that exposes normally
hardcoded internal encoder decisions (per-band joint stereo, SBR density, PS
internals, TNS/PNS, ATH/masking, block switching, verbose dump, quasi-VBR, and
more) as human-readable command-line switches.

- Author: **Michał Dziwisz**
- Subject-matter consultant: **Patryk Faliszewski**
- Built on open source: **libfdk-aac** (Fraunhofer IIS) + the **nu774/fdkaac** frontend.

> This is a Third-Party Modified Version of the Fraunhofer FDK AAC Codec Library.
> See `NOTICE.fdk-aac` and `CHANGES.md`.

## Documentation

- `README.md` — full reference (English on top, Polish below). Also as
  `README.html` / `README.pdf` / `README.rtf`.
- `MANUAL.en.md` / `MANUAL.pl.md` — the audio-engineer's manual (EN and PL),
  each also as `.html` / `.pdf` / `.rtf`. The PDFs are tagged/accessible (PDF/UA).

## Download (easiest — no account needed)

Prebuilt Windows binaries are published as **GitHub Releases**, built and hosted
by GitHub:

**→ https://github.com/michaldziwisz/franken-fdk-aac/releases/latest**

Download `franken-fdk-aac-x64-vX.Y.Z.zip` (64-bit) or the `x86` one (32-bit).
Each zip contains the encoder `.exe` plus the full documentation (README +
manuals in EN/PL, NOTICE, CHANGES). Release assets download **without logging in**.

## Building it yourself — on demand (no binaries in the repo)

The repository ships **source only**; no `.exe` is committed. Besides the
Releases above, a logged-in user with repo access can trigger a build:

1. Open the **Actions** tab of this repository.
2. In the left list pick the workflow **"Build Franken FDK AAC (Windows x64 + x86)"**.
3. Click **"Run workflow"** (top right). Optionally choose `both` / `x64` / `x86`
   (default `both`).
4. Wait for the run to finish (green check), open it, and download the
   **Artifacts**: `franken-fdk-aac-x64` and/or `franken-fdk-aac-x86`
   (each contains the `.exe` + docs). Artifacts are kept for 90 days; just run the
   workflow again if they expire. (Artifacts, unlike Release assets, require login.)

You can also build locally with mingw-w64 — see the build section in `README.md`.

## ⚠️ Patent notice

AAC is covered by patents that are still in force. Based on the SEC-disclosed
patent list, the last **baseline AAC (AAC-LC)** patent expires in **2028**, and
the last patent covering all **AAC extensions (HE-AAC / HE-AAC v2 / SBR / PS)**
expires in **2031**.

The FDK AAC copyright license (see `NOTICE.fdk-aac`, section 3) grants **NO
patent license**. Distributing or using an AAC encoder may require a separate
patent license from the relevant patent holders (via the AAC patent pool or
directly), depending on your jurisdiction and use case.

This project distributes **source code only** and builds binaries **on the
user's explicit request** on GitHub's infrastructure. By requesting a build and
using the resulting encoder, **you are responsible** for any patent licensing
that applies to you. Nothing here is legal advice.

## Licensing

- `NOTICE.fdk-aac` — the Fraunhofer FDK AAC Codec license (applies to the FDK
  sources in `src-fdk-aac/` and modifications thereto). It permits source and
  binary redistribution without copyright fees under its conditions, and
  requires that modified versions state they were changed (see `CHANGES.md`).
- `src-fdkaac/` — the nu774/fdkaac frontend, under its own license (see that
  directory).
- Retain the complete FDK license text in any redistribution, per its terms.
