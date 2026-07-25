#!/usr/bin/env python3
"""
gen_test_xpad.py - reference X-PAD generator for round-trip testing dls_decode.py.

Mirrors dab_pad_build_dls() in /mnt/d/projekty/aacfdk/src-fdkaac/src/dab_pad.c
byte-for-byte (reverse-filled X-PAD layout, CI list, DLS data-group segments with
CRC-16, 2-byte F-PAD, trailing length byte). Independent Python re-implementation
of the C encoder so the decode can be validated end-to-end.

Usage:
    python3 gen_test_xpad.py <label> <pad_size> <out_file> [toggle]
"""
import sys

PAD_SUBFIELD_LENS = [4, 6, 8, 12, 16, 24, 32, 48]
DLS_SEG_MAX = 16


def crc16_ccitt(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8) & 0xFFFF
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return (~crc) & 0xFFFF


def dls_segment(text_bytes, first_seg, last_seg, seg_index, toggle, charset=0):
    p0 = ((0x80 if toggle else 0) | (0x40 if first_seg else 0) |
          (0x20 if last_seg else 0) | (len(text_bytes) - 1))
    p1 = ((charset if first_seg else seg_index) << 4)
    dg = bytearray([p0, p1]) + bytearray(text_bytes)
    crc = crc16_ccitt(dg)
    dg.append((crc >> 8) & 0xFF)
    dg.append(crc & 0xFF)
    return bytes(dg)


def build_dls(label_bytes, pad_size, toggle=0):
    xpad_max = pad_size - 2
    text_len = len(label_bytes)
    seg_count = text_len // DLS_SEG_MAX + (1 if text_len % DLS_SEG_MAX else 0)
    if seg_count == 0:
        seg_count = 1
    max_cis = 4
    if seg_count > max_cis:
        raise ValueError("label too long for one PAD (PoC)")

    subfields = bytearray()
    ci_type = []
    ci_len_index = []
    xpad_size = 0
    used_cis = 0

    for s in range(seg_count):
        off = s * DLS_SEG_MAX
        seg_len = text_len - off
        if seg_len > DLS_SEG_MAX:
            seg_len = DLS_SEG_MAX
        if seg_len < 1:
            seg_len = 1
        dg = dls_segment(label_bytes[off:off + seg_len], s == 0,
                         s == seg_count - 1, s, toggle, 0)
        dg_len = len(dg)
        li = 0
        while li + 1 < 8 and PAD_SUBFIELD_LENS[li] < dg_len:
            li += 1
        if PAD_SUBFIELD_LENS[li] < dg_len:
            raise ValueError("won't fit")
        need = 2 if used_cis == 0 else 1
        if used_cis == max_cis - 1:
            need = 1
        if xpad_size + need + PAD_SUBFIELD_LENS[li] > xpad_max:
            raise ValueError("pad too small")
        subfields += dg
        subfields += bytes(PAD_SUBFIELD_LENS[li] - dg_len)   # zero pad
        ci_type.append(2)
        ci_len_index.append(li)
        xpad_size += need + PAD_SUBFIELD_LENS[li]
        used_cis += 1

    out = bytearray(pad_size + 1)
    pad_offset = xpad_max
    for i in range(used_cis):
        pad_offset -= 1
        out[pad_offset] = ((ci_len_index[i] << 5) | ci_type[i]) & 0xFF
    if used_cis < max_cis:
        pad_offset -= 1
        out[pad_offset] = 0x00
    for i in range(len(subfields)):
        pad_offset -= 1
        out[pad_offset] = subfields[i]
    out[xpad_max + 0] = 0x20
    out[xpad_max + 1] = 0x02 if used_cis > 0 else 0x00
    out[pad_size] = (xpad_size + 2) & 0xFF
    return bytes(out)


def main(argv):
    if len(argv) < 4:
        sys.stderr.write("usage: gen_test_xpad.py <label> <pad_size> <out_file> [toggle]\n")
        return 2
    label = argv[1].encode("latin-1")
    pad_size = int(argv[2])
    out_file = argv[3]
    toggle = int(argv[4]) if len(argv) > 4 else 0
    buf = build_dls(label, pad_size, toggle)
    with open(out_file, "wb") as f:
        f.write(buf)
    sys.stderr.write("wrote %d bytes (pad_size=%d) to %s\n"
                     % (len(buf), pad_size, out_file))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
