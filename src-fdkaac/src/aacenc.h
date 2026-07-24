/* 
 * Copyright (C) 2013 nu774
 * For conditions of distribution and use, see copyright notice in COPYING
 */
#ifndef AACENC_H
#define AACENC_H

#include <fdk-aac/aacenc_lib.h>
#include "lpcm.h"

#define AACENC_PARAMS \
    unsigned profile; \
    unsigned bitrate; \
    unsigned bitrate_mode; \
    unsigned bandwidth; \
    unsigned afterburner; \
    unsigned lowdelay_sbr; \
    unsigned sbr_ratio; \
    unsigned sbr_signaling; \
    unsigned transport_format; \
    unsigned adts_crc_check; \
    unsigned header_period; \
    int fr_ms_mask; \
    int fr_ms_bands; \
    int fr_is; \
    int fr_is_bands; \
    int fr_is_minsfbs; \
    int fr_is_corr; \
    int fr_is_lrratio; \
    int fr_core_cutoff; \
    int fr_sbr_start; \
    int fr_sbr_stop; \
    int fr_sbr_freqscale; \
    int fr_sbr_alterscale; \
    int fr_sbr_noise_bands; \
    int fr_sbr_amp_res; \
    int fr_sbr_data_extra; \
    int fr_ps; \
    int fr_ps_iid_quant; \
    int fr_tns_mask; \
    int fr_tns_order; \
    int fr_pns; \
    int fr_pns_start; \
    int fr_ath_scale; \
    int fr_block_bias; \
    int fr_vbr_reservoir; \
    int fr_max_bits_frame; \
    int fr_min_bits_frame; \
    int fr_bitres_mode; \
    int fr_ms_bias; \
    int fr_uncap_bw; \
    int fr_is_aggression; \
    int fr_force_pns; \
    int fr_unlock_bitrate; \
    int fr_speech; \
    int fr_spread_mask; \
    int fr_ms_band_lo; \
    int fr_ms_band_hi; \
    int fr_ms_precision; \
    int fr_sbr_num_env; \
    int fr_sbr_freqres_fixfix; \
    int fr_sbr_stereo_mode; \
    int fr_sbr_invf; \
    int fr_sbr_noise_floor_offset; \
    int fr_ps_icc; \
    int fr_ps_icc_mode; \
    int fr_is_band_lo; \
    int fr_is_band_hi; \
    int fr_is_force_lo; \
    int fr_is_force_hi; \
    int fr_minsnr_scale; \
    int fr_minsnr_clamp_hi; \
    int fr_minsnr_clamp_lo; \
    int fr_reduce_clamp; \
    int fr_mid_bias; \
    int fr_sbr_header_period; \
    int fr_pns_gain; \
    int fr_pns_tonality; \
    int fr_pns_refpower; \
    int fr_pns_gapfill; \
    int fr_pns_min_width; \
    int fr_peak_bitrate; \
    int fr_verbose;

typedef struct aacenc_param_t {
    AACENC_PARAMS
} aacenc_param_t;

typedef struct aacenc_frame_t {
    uint8_t *data;
    uint32_t size, capacity;
} aacenc_frame_t;

int aacenc_is_explicit_bw_compatible_sbr_signaling_available();

int aacenc_is_sbr_ratio_available();

int aacenc_is_sbr_active(const aacenc_param_t *params);

int aacenc_is_dual_rate_sbr(const aacenc_param_t *params);

void aacenc_get_lib_info(LIB_INFO *info);

int aacenc_mp4asc(const aacenc_param_t *params,
                  const uint8_t *asc, uint32_t ascsize,
                  uint8_t *outasc, uint32_t *outsize);

int aacenc_init(HANDLE_AACENCODER *encoder, const aacenc_param_t *params,
                const pcm_sample_description_t *format,
                AACENC_InfoStruct *info);

int aac_encode_frame(HANDLE_AACENCODER encoder,
                     const pcm_sample_description_t *format,
                     const INT_PCM *input, unsigned iframes,
                     aacenc_frame_t *output);

#endif
