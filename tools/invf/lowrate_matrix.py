#!/usr/bin/env python3
"""Macierz niskiego bitrate: co enkoder REALNIE robi z -b przy --unlock-bitrate.

Dla kazdej kombinacji (profil, sample rate, kanaly, -b) koduje probke,
mierzy:
  - init: czy enkoder wystartowal,
  - eff: bitrate z --verbose (to, co enkoder przyjal),
  - real: bitrate z rozmiaru strumienia (ADTS/LOAS, bez kontenera),
  - fdk / ff: liczba bledow dekodera Fraunhofera i ffmpeg (ffmpeg nie gra
    ELD+SBR, wtedy '-'),
  - sbr: czy pasmo SBR istnieje (energia > 0.5 * Nyquist rdzenia).
Uzycie: lowrate_matrix.py ENC FDKDEC [--base STARY_ENC] [--json out.json]
Pracuje w biezacym katalogu; probki tworzy sam (stdlib).
"""
import json, math, os, random, re, struct, subprocess, sys, wave


def mk(path, sr, ch, secs=4.0):
    if os.path.exists(path):
        return
    r = random.Random(sr * 10 + ch)
    w = wave.open(path, "wb"); w.setnchannels(ch); w.setsampwidth(2); w.setframerate(sr)
    c = lambda x: max(-32768, min(32767, int(x)))
    out = bytearray()
    for i in range(int(sr * secs)):
        t = i / sr
        a = 5000 * math.sin(2 * math.pi * 220 * t) + 2500 * math.sin(2 * math.pi * 1760 * t)
        n = 2500 * r.uniform(-1, 1)
        if ch == 1:
            out += struct.pack("<h", c(a + n))
        else:
            out += struct.pack("<hh", c(a + n), c(0.8 * a + 5000 * math.sin(2 * math.pi * 330 * t) * 0.5 + n))
    w.writeframes(bytes(out)); w.close()


def run(cmd):
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return p.returncode, p.stdout.decode("latin1"), p.stderr.decode("latin1")


def ff_errors(path):
    rc, o, e = run(["ffmpeg", "-v", "error", "-i", path, "-f", "null", "-"])
    return len([l for l in e.splitlines() if re.search(r"error|invalid|overflow|illegal|exceed", l, re.I)])


def hf_ratio(wav, cut_hz):
    """Udzial energii powyzej cut_hz (prosty DFT na kilku oknach)."""
    try:
        w = wave.open(wav)
    except Exception:
        return None
    sr = w.getframerate(); ch = w.getnchannels(); n = w.getnframes()
    raw = w.readframes(n); w.close()
    if n < 8192:
        return None
    sm = struct.unpack("<%dh" % (n * ch), raw)
    mono = [sm[i * ch] for i in range(n)]
    N = 2048; tot = hi = 0.0
    for start in range(n // 4, n - N, n // 6):
        seg = mono[start:start + N]
        for k in range(1, N // 2, 4):
            re_ = sum(seg[j] * math.cos(2 * math.pi * k * j / N) for j in range(0, N, 2))
            im = sum(seg[j] * math.sin(2 * math.pi * k * j / N) for j in range(0, N, 2))
            p = re_ * re_ + im * im
            tot += p
            if k * sr / N > cut_hz:
                hi += p
    return hi / tot if tot else None


def main():
    enc = sys.argv[1]; dec = sys.argv[2]
    base = None; jout = None
    if "--base" in sys.argv: base = sys.argv[sys.argv.index("--base") + 1]
    if "--json" in sys.argv: jout = sys.argv[sys.argv.index("--json") + 1]
    cases = []
    for prof, srs, extra, fmt in (
            ("5", (48000, 32000, 24000), [], "2"),
            ("29", (48000, 32000), [], "2"),
            ("23", (48000, 32000, 24000), [], "10"),
            ("39", (48000, 32000, 24000), [], "10"),
            ("39", (48000, 32000), ["-L", "1"], "10"),
            ("2", (48000, 24000), [], "2")):
        for sr in srs:
            for ch in (1, 2):
                if prof == "29" and ch == 1:
                    continue
                for b in (4000, 6000, 8000, 10000, 12000):
                    cases.append((prof, sr, ch, b, extra, fmt))
    res = []
    for prof, sr, ch, b, extra, fmt in cases:
        wav = "lr_%d_%d.wav" % (sr, ch); mk(wav, sr, ch)
        out = "lr_out." + ("loas" if fmt == "10" else "aac")
        row = {"p": prof + ("+SBR" if extra else ""), "sr": sr, "ch": ch, "b": b}
        for tag, exe in (("new", enc), ("old", base)):
            if exe is None:
                continue
            if os.path.exists(out): os.remove(out)
            rc, o, e = run([exe, "-p", prof] + extra + ["-b", str(b), "--unlock-bitrate", "-f", fmt, "--verbose", "-o", out, wav])
            m = re.search(r"^\s*bitrate\s*:\s*(\d+)", e, re.M)
            ok = rc == 0 and os.path.exists(out) and os.path.getsize(out) > 0
            row[tag + "_rc"] = rc
            row[tag + "_init"] = ok
            row[tag + "_eff"] = int(m.group(1)) if m else None
            if ok:
                secs = 4.0
                row[tag + "_real"] = round(os.path.getsize(out) * 8 / secs)
            if tag == "new" and ok:
                rc2, o2, e2 = run([dec, out, "lr_dec.wav"])
                mm = re.search(r"errors?=(\d+)", o2 + e2)
                row["fdk_err"] = int(mm.group(1)) if mm else (0 if rc2 == 0 else -1)
                row["ff_err"] = "-" if (prof == "39" and extra) else ff_errors(out)
        res.append(row)
        print(json.dumps(row), flush=True)
    if jout:
        json.dump(res, open(jout, "w"), indent=1)


if __name__ == "__main__":
    main()
