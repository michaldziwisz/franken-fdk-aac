/*
 * dab_rs.h - Reed-Solomon RS(120,110) encoder for DAB+ superframes.
 * See dab_rs.c for the DAB+ (ETSI TS 102 563) parameters.
 */
#ifndef DAB_RS_H
#define DAB_RS_H

/* Encode 110 data bytes -> 10 parity bytes (shortened RS(120,110) over GF(256)). */
void dab_rs_encode(const unsigned char *data, unsigned char *parity);

#endif /* DAB_RS_H */
