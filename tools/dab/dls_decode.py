#!/usr/bin/env python3
"""
dls_decode.py - standalone DAB+ Dynamic Label Segment (DLS) decoder.

Independent verification tool. Reads a binary file of concatenated X-PAD buffers
(each exactly pad_size+1 bytes, byte-for-byte as ODR PADPacketizer::FlushPAD /
the fdkaac dab_pad_build_dls() encoder produces: X-PAD payload + 2 F-PAD bytes +
1 trailing length byte) and prints the reassembled DLS label text to stdout.

This is the EXACT inverse of /mnt/d/projekty/aacfdk/src-fdkaac/src/dab_pad.c
(dab_pad_build_dls), including the reverse-filled X-PAD layout.

Usage:
    python3 dls_decode.py <file> <pad_size>

    <file>     binary file with one or more X-PAD buffers back-to-back
    <pad_size> the --dab-pad-len value (X-PAD payload size); each record in the
               file is pad_size+1 bytes.

Exit code 0 on success (label printed), non-zero on parse/CRC error.
"""
import sys

# ETSI X-PAD sub-field length grid, indexed by CI len_index.
PAD_SUBFIELD_LENS = [4, 6, 8, 12, 16, 24, 32, 48]

# APPTYPE values for DLS in the CI list.
APPTYPE_DLS_START = 2
APPTYPE_DLS_CONT = 3


def crc16_ccitt(data):
    """CRC-16 CCITT (poly 0x1021, init 0xFFFF, MSB-first), complemented.
    Exactly matches pad_crc16() in dab_pad.c."""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return (~crc) & 0xFFFF


def decode_dls_segment(subfield):
    """Decode one DLS data-group segment out of a zero-padded sub-field.
    Returns dict {first, last, seg_index, charset, toggle, text} or raises."""
    if len(subfield) < 4:
        raise ValueError("sub-field too short for a DLS segment")
    p0 = subfield[0]
    p1 = subfield[1]
    toggle = (p0 >> 7) & 1
    first = (p0 >> 6) & 1
    last = (p0 >> 5) & 1
    seg_len = (p0 & 0x1F) + 1
    if first:
        charset = (p1 >> 4) & 0x0F
        seg_index = 0
    else:
        charset = None
        seg_index = (p1 >> 4) & 0x0F
    dg_len = 2 + seg_len          # prefix + chars, before CRC
    if len(subfield) < dg_len + 2:
        raise ValueError("sub-field too short for declared segment length")
    text_bytes = subfield[2:2 + seg_len]
    crc_stored = (subfield[dg_len] << 8) | subfield[dg_len + 1]
    crc_calc = crc16_ccitt(subfield[:dg_len])
    if crc_stored != crc_calc:
        raise ValueError(
            "CRC mismatch: stored=0x%04X calc=0x%04X" % (crc_stored, crc_calc))
    return {
        "first": first,
        "last": last,
        "seg_index": seg_index,
        "charset": charset,
        "toggle": toggle,
        "text": text_bytes,
    }


def parse_xpad_record(buf, pad_size):
    """Parse ONE X-PAD record (pad_size+1 bytes). Returns list of decoded
    DLS segments found in this PAD (in CI order)."""
    if len(buf) != pad_size + 1:
        raise ValueError("record length %d != pad_size+1 (%d)"
                         % (len(buf), pad_size + 1))
    xpad_max = pad_size - 2                    # F-PAD sits at [xpad_max], [xpad_max+1]

    length_byte = buf[pad_size]                # total used PAD length (X-PAD + 2 F-PAD)
    fpad0 = buf[xpad_max]
    fpad1 = buf[xpad_max + 1]
    if fpad0 != 0x20:
        raise ValueError("F-PAD byte0 = 0x%02X, expected 0x20" % fpad0)
    ci_present = (fpad1 & 0x02) != 0
    if not ci_present:
        return []                             # no CI list => no X-PAD content here
    xpad_size = length_byte - 2

    # --- CI list: written reverse from offset (xpad_max-1) downward ---
    off = xpad_max - 1
    cis = []                                   # list of (len_index, apptype)
    for _ in range(4):                         # variable X-PAD => max 4 CIs
        if off < 0:
            break
        ci = buf[off]
        off -= 1
        if ci == 0x00:                         # end marker
            break
        len_index = (ci >> 5) & 0x07
        apptype = ci & 0x1F
        cis.append((len_index, apptype))

    # --- sub-fields: written reverse from current offset downward ---
    # subfields[j] (encoder order) == buf[off - j]
    total_sf = sum(PAD_SUBFIELD_LENS[li] for li, _ in cis)
    subfields = bytes(buf[off - j] for j in range(total_sf))

    # --- split sub-fields per CI, decode DLS ones ---
    segments = []
    pos = 0
    for len_index, apptype in cis:
        sflen = PAD_SUBFIELD_LENS[len_index]
        sub = subfields[pos:pos + sflen]
        pos += sflen
        if apptype in (APPTYPE_DLS_START, APPTYPE_DLS_CONT):
            segments.append(decode_dls_segment(sub))
    return segments


def reassemble(segments):
    """Reassemble ordered DLS segments into the full label string.
    Uses first/last flags + seg_index ordering."""
    by_index = {}
    charset = 0
    have_last_index = None
    for seg in segments:
        if seg["first"]:
            by_index[0] = seg
            if seg["charset"] is not None:
                charset = seg["charset"]
        else:
            by_index[seg["seg_index"]] = seg
        if seg["last"]:
            # seg_index for first-and-last is 0
            have_last_index = 0 if seg["first"] else seg["seg_index"]
    # collect contiguous indices 0..last
    if have_last_index is None:
        # fall back to whatever we have, in index order
        indices = sorted(by_index)
    else:
        indices = list(range(have_last_index + 1))
    out = bytearray()
    for i in indices:
        if i not in by_index:
            raise ValueError("missing DLS segment index %d" % i)
        out += by_index[i]["text"]
    # EBU Latin (charset 0) / UTF-8 fallback: for the ASCII PoC range these
    # coincide; decode as latin-1 to be byte-exact for the test label.
    return out.decode("latin-1")


def main(argv):
    if len(argv) != 3:
        sys.stderr.write("usage: dls_decode.py <file> <pad_size>\n")
        return 2
    path = argv[1]
    pad_size = int(argv[2])
    rec_len = pad_size + 1
    with open(path, "rb") as f:
        data = f.read()
    if len(data) % rec_len != 0:
        sys.stderr.write(
            "warning: file size %d not a multiple of pad_size+1 (%d)\n"
            % (len(data), rec_len))
    all_segments = []
    n_records = len(data) // rec_len
    for r in range(n_records):
        rec = data[r * rec_len:(r + 1) * rec_len]
        all_segments.extend(parse_xpad_record(rec, pad_size))
    if not all_segments:
        sys.stderr.write("no DLS segments found\n")
        return 1
    label = reassemble(all_segments)
    sys.stdout.write(label + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
