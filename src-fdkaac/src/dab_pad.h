/*
 * dab_pad.h - DAB+ static DLS (Dynamic Label) PAD builder. See dab_pad.c.
 */
#ifndef DAB_PAD_H
#define DAB_PAD_H

/* Build an X-PAD buffer carrying a static DLS label.
 * out       : receives pad_size+1 bytes (X-PAD payload + F-PAD, last byte = used len).
 * pad_size  : X-PAD size in bytes (F-PAD included), e.g. 16..196.
 * label     : NUL-terminated UTF-8/Latin label text.
 * toggle    : DLS toggle bit (0/1); flip only when the label text changes.
 * Returns pad_size+1 on success, 0 if the label does not fit one PAD. */
int dab_pad_build_dls(unsigned char *out, int pad_size, const char *label,
                      int toggle);

#endif /* DAB_PAD_H */
