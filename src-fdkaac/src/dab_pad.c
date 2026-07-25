/*
 * dab_pad.c - DAB+ PAD (Programme Associated Data) builder for the --dab encoder.
 *
 * PoC scope: a STATIC Dynamic Label Segment (DLS, ETSI TS 102 980) carried in a
 * Variable Size X-PAD. The label text is packed into DLS data-group segments
 * (<=16 chars each), each wrapped with a CRC, then laid out as an X-PAD field
 * (CI list + sub-fields, reverse-filled) followed by the 2-byte F-PAD and a
 * trailing length byte - byte-for-byte the buffer layout ODR-PadEnc feeds to the
 * FDK encoder via IN_ANCILLRY_DATA (see ODR PADPacketizer::FlushPAD).
 *
 * ARCHITECTURE NOTE (future dynamic PAD): the byte building here is intentionally
 * isolated from any data source. dab_pad_build_dls() takes a plain string and a
 * toggle bit; a future dynamic mode only needs to swap where that string comes
 * from (socket / metadata file) and flip the toggle when it changes - no change
 * to the packetization below.
 *
 * X-PAD sub-field length grid (ETSI): {4,6,8,12,16,24,32,48}.
 */
#include "dab_pad.h"
#include <string.h>

/* CRC-16 CCITT (poly 0x1021, init 0xFFFF, bit-by-bit MSB-first), as used by
 * DAB PAD data groups (odr::crc16). Result is complemented before appending. */
static unsigned short pad_crc16(const unsigned char *data, int len)
{
    unsigned short crc = 0xFFFF;
    int i, b;
    for (i = 0; i < len; i++) {
        crc ^= (unsigned short)data[i] << 8;
        for (b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (unsigned short)((crc << 1) ^ 0x1021)
                                 : (unsigned short)(crc << 1);
    }
    return (unsigned short)~crc;
}

static const int PAD_SUBFIELD_LENS[8] = {4, 6, 8, 12, 16, 24, 32, 48};

/* Build one DLS segment data group (prefix[2] + chars + CRC[2]) into out.
 * Returns data-group length (incl CRC). charset=0 (EBU Latin). */
static int dls_segment(unsigned char *out, const char *text, int seg_text_len,
                       int first_seg, int last_seg, int seg_index, int toggle,
                       int charset)
{
    int n = 0;
    out[0] = (unsigned char)((toggle ? 0x80 : 0) | (first_seg ? 0x40 : 0) |
                             (last_seg ? 0x20 : 0) | (seg_text_len - 1));
    out[1] = (unsigned char)((first_seg ? charset : seg_index) << 4);
    memcpy(&out[2], text, seg_text_len);
    n = 2 + seg_text_len;
    {
        unsigned short crc = pad_crc16(out, n);
        out[n++] = (unsigned char)((crc >> 8) & 0xFF);
        out[n++] = (unsigned char)(crc & 0xFF);
    }
    return n;
}

/*
 * Build a full X-PAD buffer for a static DLS label.
 *   pad_size : target X-PAD payload size in bytes (the --dab-pad-len value; the
 *              buffer written is pad_size+1, last byte = used length, as FDK/ODR expect).
 *   toggle   : DLS toggle bit (flip when the label text changes; static => constant).
 * Returns total bytes written to out (== pad_size+1), or 0 on error (text too long
 * for one PAD / pad_size too small). out must hold at least pad_size+1 bytes.
 *
 * PoC constraint: the whole label must fit in ONE X-PAD (all DLS segments packed
 * into the available sub-fields of a single PAD). For labels up to ~pad_size-8
 * chars this holds; longer labels need the multi-PAD packetizer (future work).
 */
int dab_pad_build_dls(unsigned char *out, int pad_size, const char *label,
                      int toggle)
{
    const int DLS_SEG_MAX = 16;
    const int xpad_max = pad_size - 2;      /* minus F-PAD */
    int text_len = (int)strlen(label);
    int seg_count = text_len / DLS_SEG_MAX + (text_len % DLS_SEG_MAX ? 1 : 0);
    unsigned char subfields[4 * 48];
    int subfields_size = 0;
    int ci_type[4], ci_len_index[4], used_cis = 0;
    int max_cis = 4;                        /* variable-size X-PAD */
    int xpad_size = 0;
    int s, i, pad_offset;

    if (seg_count == 0) seg_count = 1;      /* empty label => one empty segment */
    if (seg_count > max_cis) return 0;      /* PoC: one PAD only */

    /* Build each DLS segment, assign it a CI + sub-field. */
    for (s = 0; s < seg_count; s++) {
        unsigned char dg[2 + 16 + 2];
        int off = s * DLS_SEG_MAX;
        int seg_len = text_len - off;
        int dg_len, li, need;
        if (seg_len > DLS_SEG_MAX) seg_len = DLS_SEG_MAX;
        if (seg_len < 1) seg_len = 1;       /* min 1 char per segment */
        dg_len = dls_segment(dg, label + off, seg_len, s == 0,
                             s == seg_count - 1, s, toggle, 0);

        /* smallest sub-field that holds the whole data group */
        li = 0;
        while (li + 1 < 8 && PAD_SUBFIELD_LENS[li] < dg_len) li++;
        if (PAD_SUBFIELD_LENS[li] < dg_len) return 0;   /* won't fit */

        /* CI cost: first CI in variable X-PAD also needs the end marker (2), else 1 */
        need = (used_cis == 0) ? 2 : 1;
        if (used_cis == max_cis - 1) need = 1;
        if (xpad_size + need + PAD_SUBFIELD_LENS[li] > xpad_max) return 0;

        /* write data group into sub-field, zero-padded to sub-field length */
        memcpy(&subfields[subfields_size], dg, dg_len);
        memset(&subfields[subfields_size + dg_len], 0,
               PAD_SUBFIELD_LENS[li] - dg_len);
        subfields_size += PAD_SUBFIELD_LENS[li];

        ci_type[used_cis] = 2;              /* APPTYPE DLS start = 2 (cont = 3) */
        ci_len_index[used_cis] = li;
        xpad_size += need + PAD_SUBFIELD_LENS[li];
        used_cis++;
    }

    /* Lay out the PAD reverse-filled, exactly like ODR PADPacketizer::FlushPAD. */
    memset(out, 0, pad_size + 1);
    pad_offset = xpad_max;
    /* CI list (each CI: len_index<<5 | apptype) */
    for (i = 0; i < used_cis; i++)
        out[--pad_offset] = (unsigned char)((ci_len_index[i] << 5) | ci_type[i]);
    if (used_cis < max_cis)
        out[--pad_offset] = 0x00;          /* end marker */
    /* sub-fields */
    for (i = 0; i < subfields_size; i++)
        out[--pad_offset] = subfields[i];

    /* F-PAD (2 bytes): X-PAD present, variable size, with CI list. */
    out[xpad_max + 0] = 0x20;
    out[xpad_max + 1] = (unsigned char)(used_cis > 0 ? 0x02 : 0x00);
    /* trailing byte: total used PAD length (X-PAD used + F-PAD) */
    out[pad_size] = (unsigned char)(xpad_size + 2);

    return pad_size + 1;
}
