/* 
 * Copyright (C) 2013 nu774
 * For conditions of distribution and use, see copyright notice in COPYING
 */
#if HAVE_CONFIG_H
#  include "config.h"
#endif
#if HAVE_STDINT_H
#  include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aacenc.h"

int aacenc_is_explicit_bw_compatible_sbr_signaling_available()
{
    LIB_INFO lib_info;
    aacenc_get_lib_info(&lib_info);
    return lib_info.version > 0x03040900;
}

int aacenc_is_sbr_ratio_available()
{
#if AACENCODER_LIB_VL0 < 3 || (AACENCODER_LIB_VL0==3 && AACENCODER_LIB_VL1<4)
    return 0;
#else
    LIB_INFO lib_info;
    aacenc_get_lib_info(&lib_info);
    return lib_info.version > 0x03040800;
#endif
}

int aacenc_is_sbr_active(const aacenc_param_t *params)
{
    switch (params->profile) {
    case AOT_SBR: case AOT_PS:
    case AOT_DRM_SBR: case AOT_DRM_MPEG_PS:
        return 1;
    }
    if (params->profile == AOT_ER_AAC_ELD && params->lowdelay_sbr)
        return 1;
    return 0;
}

int aacenc_is_dual_rate_sbr(const aacenc_param_t *params)
{
    if (params->profile == AOT_PS)
        return 1;
    else if (params->profile == AOT_SBR)
        return params->sbr_ratio == 0 || params->sbr_ratio == 2;
    else if (params->profile == AOT_ER_AAC_ELD && params->lowdelay_sbr)
        return params->sbr_ratio == 2;
    return 0;
}

void aacenc_get_lib_info(LIB_INFO *info)
{
    LIB_INFO *lib_info = 0;
    lib_info = calloc(FDK_MODULE_LAST, sizeof(LIB_INFO));
    if (aacEncGetLibInfo(lib_info) == AACENC_OK) {
        int i;
        for (i = 0; i < FDK_MODULE_LAST; ++i) {
            if (lib_info[i].module_id == FDK_AACENC) {
                memcpy(info, &lib_info[i], sizeof(LIB_INFO));
                break;
            }
        }
    }
    free(lib_info);
}

static const unsigned aacenc_sampling_freq_tab[] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 
    16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

static
unsigned sampling_freq_index(unsigned rate)
{
    unsigned i;
    for (i = 0; aacenc_sampling_freq_tab[i]; ++i)
        if (aacenc_sampling_freq_tab[i] == rate)
            return i;
    return 0xf;
}

/*
 * Append backward compatible SBR/PS signaling to implicit signaling ASC,
 * if SBR/PS is present.
 */
int aacenc_mp4asc(const aacenc_param_t *params,
                  const uint8_t *asc, uint32_t ascsize,
                  uint8_t *outasc, uint32_t *outsize)
{
    unsigned asc_sfreq = aacenc_sampling_freq_tab[(asc[0]&0x7)<<1 |asc[1]>>7];
    unsigned shift = aacenc_is_dual_rate_sbr(params);

    switch (params->profile) {
    case AOT_SBR:
    case AOT_PS:
        if (!shift)
            break;
        if (*outsize < ascsize + 3)
            return -1;
        memcpy(outasc, asc, ascsize);
        /* syncExtensionType:11 (value:0x2b7) */
        outasc[ascsize+0] = 0x2b << 1;
        outasc[ascsize+1] = 0x7 << 5;
        /* extensionAudioObjectType:5 (value:5)*/
        outasc[ascsize+1] |= 5;
        /* sbrPresentFlag:1 (value:1) */
        outasc[ascsize+2] = 0x80;
        /* extensionSamplingFrequencyIndex:4 */
        outasc[ascsize+2] |= sampling_freq_index(asc_sfreq << shift) << 3;
        if (params->profile == AOT_SBR) {
            *outsize = ascsize + 3;
            return 0;
        }
        if (*outsize < ascsize + 5)
            return -1;
        /* syncExtensionType:11 (value:0x548) */
        outasc[ascsize+2] |= 0x5;
        outasc[ascsize+3] = 0x48;
        /* psPresentFlag:1 (value:1) */
        outasc[ascsize+4] = 0x80;
        *outsize = ascsize + 5;
        return 0;
    }
    if (*outsize < ascsize)
        return -1;
    memcpy(outasc, asc, ascsize);
    *outsize = ascsize;
    return 0;
}

static
int aacenc_channel_mode(const pcm_sample_description_t *format)
{
    uint32_t chanmask = format->channel_mask;

    if (format->channels_per_frame > 8)
        return 0;
    if (!chanmask) {
        static uint32_t defaults[] = { 0x4, 0x3, 0x7, 0, 0x37, 0x3f, 0, 0x63f };
        chanmask = defaults[format->channels_per_frame - 1];
    }
    switch (chanmask) {
    case 0x3:   return MODE_2;
    case 0x4:   return MODE_1;
    case 0x7:   return MODE_1_2;
    case 0x37:  return MODE_1_2_2;
    case 0x3f:  return MODE_1_2_2_1;
    case 0x107: return MODE_1_2_1;
    case 0x607: return MODE_1_2_2;
    case 0x60f: return MODE_1_2_2_1;
#if AACENCODER_LIB_VL0 > 3 || (AACENCODER_LIB_VL0==3 && AACENCODER_LIB_VL1>=4)
    case 0xff:  return MODE_1_2_2_2_1;
    case 0x63f: return MODE_7_1_REAR_SURROUND;
#endif
    }
    return 0;
}

int aacenc_init(HANDLE_AACENCODER *encoder, const aacenc_param_t *params,
                const pcm_sample_description_t *format,
                AACENC_InfoStruct *info)
{
    int channel_mode;
    int aot;
    LIB_INFO lib_info;

    *encoder = 0;
    aacenc_get_lib_info(&lib_info);

    if ((channel_mode = aacenc_channel_mode(format)) == 0) {
        fprintf(stderr, "ERROR: unsupported channel layout\n");
        goto FAIL;
    }
    if (aacEncOpen(encoder, 0, 0) != AACENC_OK) {
        fprintf(stderr, "ERROR: aacEncOpen() failed\n");
        goto FAIL;
    }
    aot = (params->profile ? params->profile : AOT_AAC_LC);
    if (aacEncoder_SetParam(*encoder, AACENC_AOT, aot) != AACENC_OK) {
        fprintf(stderr, "ERROR: unsupported profile\n");
        goto FAIL;
    }
    if (params->bitrate_mode == 0)
        aacEncoder_SetParam(*encoder, AACENC_BITRATE, params->bitrate);
    else if (aacEncoder_SetParam(*encoder, AACENC_BITRATEMODE,
                                 params->bitrate_mode) != AACENC_OK) {
        fprintf(stderr, "ERROR: unsupported bitrate mode\n");
        goto FAIL;
    }
    if (aacEncoder_SetParam(*encoder, AACENC_SAMPLERATE,
                            format->sample_rate) != AACENC_OK) {
        fprintf(stderr, "ERROR: unsupported sample rate\n");
        goto FAIL;
    }
    if (aacEncoder_SetParam(*encoder, AACENC_CHANNELMODE,
                            channel_mode) != AACENC_OK) {
        fprintf(stderr, "ERROR: unsupported channel mode\n");
        goto FAIL;
    }
    aacEncoder_SetParam(*encoder, AACENC_BANDWIDTH, params->bandwidth);
    aacEncoder_SetParam(*encoder, AACENC_CHANNELORDER, 1);
    aacEncoder_SetParam(*encoder, AACENC_AFTERBURNER, !!params->afterburner);

    aacEncoder_SetParam(*encoder, AACENC_SBR_MODE, params->lowdelay_sbr);

#if AACENCODER_LIB_VL0 > 3 || (AACENCODER_LIB_VL0==3 && AACENCODER_LIB_VL1>=4)
    if (lib_info.version > 0x03040800)
        aacEncoder_SetParam(*encoder, AACENC_SBR_RATIO, params->sbr_ratio);
#endif

    if (aacEncoder_SetParam(*encoder, AACENC_TRANSMUX,
                            params->transport_format) != AACENC_OK) {
        fprintf(stderr, "ERROR: unsupported transport format\n");
        goto FAIL;
    }
    if (aacEncoder_SetParam(*encoder, AACENC_SIGNALING_MODE,
                            params->sbr_signaling) != AACENC_OK) {
        fprintf(stderr, "ERROR: failed to set SBR signaling mode\n");
        goto FAIL;
    }
    if (params->adts_crc_check)
        aacEncoder_SetParam(*encoder, AACENC_PROTECTION, 1);
    if (params->header_period)
        aacEncoder_SetParam(*encoder, AACENC_HEADER_PERIOD,
                            params->header_period);

    /* ---- Frankenstein debug/laboratory parameters ---- */
#define FR_SET(cond, p, v) do { if (cond) aacEncoder_SetParam(*encoder, (p), (unsigned)(v)); } while (0)
    FR_SET(params->fr_ms_mask       != -1, AACENC_FRANKEN_MS_MASK,       params->fr_ms_mask);
    FR_SET(params->fr_ms_bands      != -1, AACENC_FRANKEN_MS_MAXBANDS,   params->fr_ms_bands);
    FR_SET(params->fr_is            != -1, AACENC_FRANKEN_IS,            params->fr_is);
    FR_SET(params->fr_is_bands      != -1, AACENC_FRANKEN_IS_MAXBANDS,   params->fr_is_bands);
    FR_SET(params->fr_is_minsfbs    != -1, AACENC_FRANKEN_IS_MINSFBS,    params->fr_is_minsfbs);
    FR_SET(params->fr_is_corr       != -1, AACENC_FRANKEN_IS_CORRTHRESH, params->fr_is_corr);
    FR_SET(params->fr_is_lrratio    != -1, AACENC_FRANKEN_IS_LRRATIO,    params->fr_is_lrratio);
    FR_SET(params->fr_core_cutoff   !=  0, AACENC_FRANKEN_CORE_CUTOFF,   params->fr_core_cutoff);
    FR_SET(params->fr_sbr_start     != -1, AACENC_FRANKEN_SBR_START,     params->fr_sbr_start);
    FR_SET(params->fr_sbr_stop      != -1, AACENC_FRANKEN_SBR_STOP,      params->fr_sbr_stop);
    FR_SET(params->fr_sbr_freqscale != -1, AACENC_FRANKEN_SBR_FREQSCALE, params->fr_sbr_freqscale);
    FR_SET(params->fr_sbr_alterscale!= -1, AACENC_FRANKEN_SBR_ALTERSCALE,params->fr_sbr_alterscale);
    FR_SET(params->fr_sbr_noise_bands!=-1, AACENC_FRANKEN_SBR_NOISEBANDS,params->fr_sbr_noise_bands);
    FR_SET(params->fr_sbr_amp_res   != -1, AACENC_FRANKEN_SBR_AMPRES,    params->fr_sbr_amp_res);
    FR_SET(params->fr_sbr_data_extra!= -1, AACENC_FRANKEN_SBR_DATAEXTRA, params->fr_sbr_data_extra);
    FR_SET(params->fr_ps            != -1, AACENC_FRANKEN_PS,            params->fr_ps);
    FR_SET(params->fr_ps_iid_quant  != -1, AACENC_FRANKEN_PS_IIDQUANT,   params->fr_ps_iid_quant);
    FR_SET(params->fr_tns_mask      != -1, AACENC_FRANKEN_TNS_MASK,      params->fr_tns_mask);
    FR_SET(params->fr_tns_order     != -1, AACENC_FRANKEN_TNS_MAXORDER,  params->fr_tns_order);
    FR_SET(params->fr_pns           != -1, AACENC_FRANKEN_PNS,           params->fr_pns);
    FR_SET(params->fr_pns_start     != -1, AACENC_FRANKEN_PNS_START,     params->fr_pns_start);
    FR_SET(params->fr_ath_scale     != -1, AACENC_FRANKEN_ATH_SCALE,     params->fr_ath_scale);
    FR_SET(params->fr_block_bias    != -1, AACENC_FRANKEN_BLOCK_BIAS,    params->fr_block_bias);
    FR_SET(params->fr_vbr_reservoir != -1, AACENC_FRANKEN_VBR_RESERVOIR, params->fr_vbr_reservoir);
    FR_SET(params->fr_max_bits_frame!= -1, AACENC_FRANKEN_MAX_BITS_FRAME,params->fr_max_bits_frame);
    FR_SET(params->fr_min_bits_frame!= -1, AACENC_FRANKEN_MIN_BITS_FRAME,params->fr_min_bits_frame);
    FR_SET(params->fr_bitres_mode   != -1, AACENC_FRANKEN_BITRES_MODE,   params->fr_bitres_mode);
    FR_SET(params->fr_ms_bias       != -1, AACENC_FRANKEN_MS_BIAS,       params->fr_ms_bias);
    FR_SET(params->fr_uncap_bw      != -1, AACENC_FRANKEN_UNCAP_BW,      params->fr_uncap_bw);
    FR_SET(params->fr_is_aggression != -1, AACENC_FRANKEN_IS_AGGRESSION, params->fr_is_aggression);
    FR_SET(params->fr_force_pns     != -1, AACENC_FRANKEN_FORCE_PNS,     params->fr_force_pns);
    FR_SET(params->fr_unlock_bitrate!= -1, AACENC_FRANKEN_UNLOCK_BITRATE, params->fr_unlock_bitrate);
    FR_SET(params->fr_speech        != -1, AACENC_FRANKEN_SPEECH,          params->fr_speech);
    FR_SET(params->fr_spread_mask   != -1, AACENC_FRANKEN_SPREAD_MASK,     params->fr_spread_mask);
    FR_SET(params->fr_ms_band_lo != -1, AACENC_FRANKEN_MS_BAND_LO, params->fr_ms_band_lo);
    FR_SET(params->fr_ms_band_hi != -1, AACENC_FRANKEN_MS_BAND_HI, params->fr_ms_band_hi);
    FR_SET(params->fr_ms_precision != -1, AACENC_FRANKEN_MS_PRECISION, params->fr_ms_precision);
    FR_SET(params->fr_sbr_num_env != -1, AACENC_FRANKEN_SBR_NUM_ENV, params->fr_sbr_num_env);
    FR_SET(params->fr_sbr_freqres_fixfix != -1, AACENC_FRANKEN_SBR_FREQRES_FIXFIX, params->fr_sbr_freqres_fixfix);
    FR_SET(params->fr_sbr_stereo_mode != -1, AACENC_FRANKEN_SBR_STEREO_MODE, params->fr_sbr_stereo_mode);
    FR_SET(params->fr_sbr_invf != -1, AACENC_FRANKEN_SBR_INVF, params->fr_sbr_invf);
    FR_SET(params->fr_sbr_noise_floor_offset != -128, AACENC_FRANKEN_SBR_NOISE_FLOOR_OFFSET, params->fr_sbr_noise_floor_offset);
    FR_SET(params->fr_ps_icc != -1, AACENC_FRANKEN_PS_ICC, params->fr_ps_icc);
    FR_SET(params->fr_ps_icc_mode != -1, AACENC_FRANKEN_PS_ICC_MODE, params->fr_ps_icc_mode);
    FR_SET(params->fr_is_band_lo != -1, AACENC_FRANKEN_IS_BAND_LO, params->fr_is_band_lo);
    FR_SET(params->fr_is_band_hi != -1, AACENC_FRANKEN_IS_BAND_HI, params->fr_is_band_hi);
    FR_SET(params->fr_is_force_lo != -1, AACENC_FRANKEN_IS_FORCE_LO, params->fr_is_force_lo);
    FR_SET(params->fr_is_force_hi != -1, AACENC_FRANKEN_IS_FORCE_HI, params->fr_is_force_hi);
    FR_SET(params->fr_minsnr_scale != -1, AACENC_FRANKEN_MINSNR_SCALE, params->fr_minsnr_scale);
    FR_SET(params->fr_minsnr_clamp_hi != -1, AACENC_FRANKEN_MINSNR_CLAMP_HI, params->fr_minsnr_clamp_hi);
    FR_SET(params->fr_minsnr_clamp_lo != -1, AACENC_FRANKEN_MINSNR_CLAMP_LO, params->fr_minsnr_clamp_lo);
    FR_SET(params->fr_reduce_clamp != -1, AACENC_FRANKEN_REDUCE_CLAMP, params->fr_reduce_clamp);
    FR_SET(params->fr_mid_bias != -1, AACENC_FRANKEN_MID_BIAS, params->fr_mid_bias);
    FR_SET(params->fr_side_bias != AACENC_FRANKEN_OFF, AACENC_FRANKEN_SIDE_BIAS, params->fr_side_bias);
    FR_SET(params->fr_side_knee != AACENC_FRANKEN_OFF, AACENC_FRANKEN_SIDE_KNEE, params->fr_side_knee);
    FR_SET(params->fr_mask_slope != AACENC_FRANKEN_OFF, AACENC_FRANKEN_MASK_SLOPE, params->fr_mask_slope);
    FR_SET(params->fr_sbr_header_period != -1, AACENC_FRANKEN_SBR_HEADER_PERIOD, params->fr_sbr_header_period);
    FR_SET(params->fr_pns_gain != -1, AACENC_FRANKEN_PNS_GAIN, params->fr_pns_gain);
    FR_SET(params->fr_pns_tonality != -1, AACENC_FRANKEN_PNS_TONALITY, params->fr_pns_tonality);
    FR_SET(params->fr_pns_refpower != -1, AACENC_FRANKEN_PNS_REFPOWER, params->fr_pns_refpower);
    FR_SET(params->fr_pns_gapfill != -1, AACENC_FRANKEN_PNS_GAPFILL, params->fr_pns_gapfill);
    FR_SET(params->fr_pns_min_width != -1, AACENC_FRANKEN_PNS_MIN_WIDTH, params->fr_pns_min_width);
    FR_SET(params->fr_peak_bitrate  != -1, AACENC_PEAK_BITRATE,          params->fr_peak_bitrate);
    FR_SET(params->fr_verbose       !=  0, AACENC_FRANKEN_VERBOSE,       params->fr_verbose);
#undef FR_SET

    if (aacEncEncode(*encoder, 0, 0, 0, 0) != AACENC_OK) {
        fprintf(stderr, "ERROR: encoder initialization failed\n");
        goto FAIL;
    }
    if (aacEncInfo(*encoder, info) != AACENC_OK) {
        fprintf(stderr, "ERROR: cannot retrieve encoder info\n");
        goto FAIL;
    }
    if (params->fr_verbose) {
        unsigned aot = aacEncoder_GetParam(*encoder, AACENC_AOT);
        int sbr = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_SBR_ACTIVE);
        int maxsfb = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_MAXSFB);
        int useMS = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_USEMS);
        int useIS = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_USEIS);
        int useTns = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_USETNS);
        int usePns = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_USEPNS);
        fprintf(stderr, "=== fdkaac-franken: effective encoder settings ===\n");
        fprintf(stderr, "(objective values the encoder actually initialised; see README for legends)\n");
        fprintf(stderr, " AOT (profile)         : %u\n", aot);
        fprintf(stderr, " bitrate               : %u\n", aacEncoder_GetParam(*encoder, AACENC_BITRATE));
        fprintf(stderr, " bitrate-mode          : %u\n", aacEncoder_GetParam(*encoder, AACENC_BITRATEMODE));
        fprintf(stderr, " samplerate            : %u\n", aacEncoder_GetParam(*encoder, AACENC_SAMPLERATE));
        fprintf(stderr, " channel-mode          : %u\n", aacEncoder_GetParam(*encoder, AACENC_CHANNELMODE));
        { unsigned bw = aacEncoder_GetParam(*encoder, AACENC_BANDWIDTH);
          int effbw = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_BANDWIDTH_HZ);
          const char *src;
          if (params->fr_core_cutoff > 0)      src = "from --core-cutoff";
          else if (params->bandwidth > 0)      src = "from -w";
          else                                 src = "auto (FDK picked for this bitrate)";
          /* Effective cutoff is anchored to the nearest SFB boundary, so it can
           * differ from the requested -w/--core-cutoff. Show the REAL value. */
          if (effbw > 0)
            fprintf(stderr, " core bandwidth        : %d Hz (SFB-anchored)  [%s]\n", effbw, src);
          else
            fprintf(stderr, " core bandwidth        : %u Hz  [%s]\n", bw, src);
          if (sbr)
            fprintf(stderr, "                         (this is the AAC CORE cutoff; SBR extends above it)\n");
        }
        fprintf(stderr, " afterburner           : %u\n", aacEncoder_GetParam(*encoder, AACENC_AFTERBURNER));
        fprintf(stderr, " transport-format      : %u\n", aacEncoder_GetParam(*encoder, AACENC_TRANSMUX));
        { unsigned sig = aacEncoder_GetParam(*encoder, AACENC_SIGNALING_MODE);
          if (sig == (unsigned)-1) fprintf(stderr, " signaling-mode        : auto\n");
          else fprintf(stderr, " signaling-mode        : %u\n", sig); }
        /* Auto-initialised values FDK derived by itself (no franken switch) - useful
         * to understand what the library chose to hit the requested bitrate. */
        fprintf(stderr, "--- encoder auto-init (FDK-derived, read-only) ---\n");
        fprintf(stderr, " SBR mode              : %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_SBR_MODE));
        if (sbr)
            fprintf(stderr, " SBR ratio             : %u\n", aacEncoder_GetParam(*encoder, AACENC_SBR_RATIO));
        fprintf(stderr, " granule length        : %u samples/frame\n", aacEncoder_GetParam(*encoder, AACENC_GRANULE_LENGTH));
        { unsigned hp = aacEncoder_GetParam(*encoder, AACENC_HEADER_PERIOD);
          fprintf(stderr, " header period         : %u\n", hp); }
        fprintf(stderr, " ancillary bitrate     : %u\n", aacEncoder_GetParam(*encoder, AACENC_ANCILLARY_BITRATE));
        fprintf(stderr, " protection (CRC)      : %u\n", aacEncoder_GetParam(*encoder, AACENC_PROTECTION));
        { AACENC_InfoStruct info; memset(&info, 0, sizeof(info));
          if (aacEncInfo(*encoder, &info) == AACENC_OK) {
            fprintf(stderr, " frame length          : %u samples/channel\n", info.frameLength);
            fprintf(stderr, " codec delay (total)   : %u samples/channel\n", info.nDelay);
            fprintf(stderr, " codec delay (core)    : %u samples/channel\n", info.nDelayCore);
            fprintf(stderr, " max frame size        : %u bytes\n", info.maxOutBufBytes);
            fprintf(stderr, " ASC/config size       : %u bytes\n", info.confSize);
          } }
        fprintf(stderr, " active SFBs (long)    : %d  (top band index for --msbands/--isbands)\n", maxsfb);
        {
        unsigned chmode = aacEncoder_GetParam(*encoder, AACENC_CHANNELMODE);
        fprintf(stderr, "--- stereo tools ---\n");
        if (chmode == 1) {
            fprintf(stderr, " (mono core: MS/IS not applicable");
            if (aot == 29) fprintf(stderr, "; stereo image via Parametric Stereo below");
            fprintf(stderr, ")\n");
        } else {
        fprintf(stderr, " MS stereo tool        : %s", useMS ? "on" : "off");
        if (params->fr_ms_mask >= 0) fprintf(stderr, "  [forced %s]", params->fr_ms_mask ? "on" : "off");
        if (params->fr_ms_bands >= 0) fprintf(stderr, "  [max %d bands]", params->fr_ms_bands);
        else if (params->fr_ms_band_lo >= 0 || params->fr_ms_band_hi >= 0)
            fprintf(stderr, "  [band range %d..%d]", params->fr_ms_band_lo, params->fr_ms_band_hi);
        else fprintf(stderr, "  [bands: auto up to %d]", maxsfb);
        fprintf(stderr, "\n");
        fprintf(stderr, " Intensity stereo (IS) : %s", useIS ? "on" : "off");
        if (params->fr_is_bands >= 0) fprintf(stderr, "  [max %d bands]", params->fr_is_bands);
        else fprintf(stderr, "  [bands: auto up to %d]", maxsfb);
        if (params->fr_is_band_lo >= 0 || params->fr_is_band_hi >= 0)
            fprintf(stderr, "  [allowed range %d..%d]", params->fr_is_band_lo, params->fr_is_band_hi);
        if (params->fr_is_force_lo >= 0 || params->fr_is_force_hi >= 0)
            fprintf(stderr, "  [FORCED range %d..%d]", params->fr_is_force_lo, params->fr_is_force_hi);
        fprintf(stderr, "\n");
        fprintf(stderr, " IS min contiguous SFBs: %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_IS_MINSFBS));
        fprintf(stderr, " IS corr threshold     : %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_IS_CORR));
        fprintf(stderr, " IS L/R ratio thresh   : %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_IS_LRRATIO));
        }
        }
        fprintf(stderr, "--- noise shaping / substitution ---\n");
        fprintf(stderr, " TNS                   : %s", useTns ? "on" : "off");
        if (useTns) fprintf(stderr, "  mask=0x%X  max-order(long)=%d",
                            (unsigned)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_TNS_MASK),
                            (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_TNS_ORDER));
        fprintf(stderr, "\n");
        fprintf(stderr, " PNS                   : %s", usePns ? "on (requested)" : "off");
        if (usePns) {
            int ps = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_PNS_START);
            if (ps >= 0) fprintf(stderr, "  start=%d Hz", ps);
            else fprintf(stderr, "  start=auto (may be gated off at low bitrate; see --force-pns)");
        }
        fprintf(stderr, "\n");
        if (params->fr_pns_gain >= 0 || params->fr_pns_tonality >= 0 ||
            params->fr_pns_refpower >= 0 || params->fr_pns_gapfill >= 0 ||
            params->fr_pns_min_width >= 0) {
            fprintf(stderr, " PNS shaping           :");
            if (params->fr_pns_gain >= 0)     fprintf(stderr, " gain=%.2f", params->fr_pns_gain/100.0);
            if (params->fr_pns_tonality >= 0) fprintf(stderr, " tonality=%.2f", params->fr_pns_tonality/100.0);
            if (params->fr_pns_refpower >= 0) fprintf(stderr, " refpower=%.2f", params->fr_pns_refpower/100.0);
            if (params->fr_pns_gapfill >= 0)  fprintf(stderr, " gapfill=%.2f", params->fr_pns_gapfill/100.0);
            if (params->fr_pns_min_width >= 0)fprintf(stderr, " min-width=%d", params->fr_pns_min_width);
            fprintf(stderr, "\n");
        }
        if (sbr) {
            fprintf(stderr, "--- SBR (active) effective settings ---\n");
            fprintf(stderr, " sbr-ratio             : %u\n", aacEncoder_GetParam(*encoder, AACENC_SBR_RATIO));
            fprintf(stderr, " sbr start freq index  : %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_SBR_START));
            fprintf(stderr, " sbr stop freq index   : %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_SBR_STOP));
            { int stophz = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_SBR_STOP_HZ);
              if (stophz > 0)
                fprintf(stderr, " final BW (AAC+SBR)    : ~%d Hz  (top edge from SBR stop index)\n", stophz); }
            fprintf(stderr, " sbr freq scale        : %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_SBR_FREQSCALE));
            fprintf(stderr, " sbr noise bands       : %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_SBR_NOISEBANDS));
            fprintf(stderr, " sbr amp res           : %d\n", (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_SBR_AMPRES));
            { int hp = (int)aacEncoder_GetParam(*encoder, AACENC_FRANKEN_GET_SBR_HEADER_PERIOD);
              unsigned sr = aacEncoder_GetParam(*encoder, AACENC_SAMPLERATE);
              unsigned gl = aacEncoder_GetParam(*encoder, AACENC_GRANULE_LENGTH);
              if (hp > 0 && sr > 0 && gl > 0) {
                double ms = 1000.0 * (double)hp * (double)gl / (double)sr;
                fprintf(stderr, " sbr header period      : every %d frame(s) = ~%.0f ms  (SBR sync delay when tuning into a stream)\n", hp, ms);
                if (params->fr_sbr_header_period >= 1)
                    fprintf(stderr, "                          [forced via --sbr-header-period %d; 1 = near-instant SBR lock]\n", params->fr_sbr_header_period);
              }
            }
            fprintf(stderr, " sbr stereo-mode       : %s\n",
                    params->fr_sbr_stereo_mode < 0 ? "auto (default)" :
                    params->fr_sbr_stereo_mode == 0 ? "0 mono" :
                    params->fr_sbr_stereo_mode == 1 ? "1 LR (full separation)" :
                    params->fr_sbr_stereo_mode == 2 ? "2 coupling" : "3 switch-LRC");
            fprintf(stderr, " sbr inverse filtering : %s\n",
                    params->fr_sbr_invf < 0 ? "auto (tonality estimator)" :
                    params->fr_sbr_invf == 0 ? "0 off (forced)" :
                    params->fr_sbr_invf == 1 ? "1 low (forced)" :
                    params->fr_sbr_invf == 2 ? "2 mid (forced)" : "3 high (forced)");
            if (params->fr_sbr_num_env > 0)
                fprintf(stderr, " sbr envelopes/frame   : %d (forced static grid)\n", params->fr_sbr_num_env);
            if (params->fr_sbr_noise_floor_offset != -128)
                fprintf(stderr, " sbr noise-floor offset: %d\n", params->fr_sbr_noise_floor_offset);
            if (params->fr_speech == 1)
                fprintf(stderr, " sbr speech tuning     : on\n");
        } else {
            fprintf(stderr, " SBR                   : not active for this profile\n");
        }
        if (aot == 29 /* HE-AAC v2 = PS */) {
            fprintf(stderr, "--- Parametric Stereo (HE-AAC v2) ---\n");
            fprintf(stderr, " PS IID sending        : %s\n",
                    params->fr_ps < 0 ? "auto (default)" : params->fr_ps ? "forced on" : "forced off");
            fprintf(stderr, " PS IID quant grid     : %s\n",
                    params->fr_ps_iid_quant < 0 ? "auto (default)" :
                    params->fr_ps_iid_quant == 0 ? "coarse" : "fine");
            fprintf(stderr, " PS ICC (coherence)    : %s\n",
                    params->fr_ps_icc < 0 ? "auto (default)" : params->fr_ps_icc ? "forced on" : "forced off");
            fprintf(stderr, " PS ICC rotation mode  : %s\n",
                    params->fr_ps_icc_mode < 0 ? "auto (default)" :
                    params->fr_ps_icc_mode == 0 ? "ROT_A" : "ROT_B");
            fprintf(stderr, " PS IPD/OPD (phase)    : not supported by FDK (always 0)\n");
        }
        /* Bit-reservoir budget helper: the hard AAC ceiling is 6144 bits per
         * channel per frame; the usable reservoir is that minus the average
         * bits already spent per frame. Showing both the mono and stereo
         * ceilings lets you size --vbr-reservoir consciously without overshoot. */
        {
            unsigned br = aacEncoder_GetParam(*encoder, AACENC_BITRATE);
            unsigned sr = aacEncoder_GetParam(*encoder, AACENC_SAMPLERATE);
            unsigned gl = aacEncoder_GetParam(*encoder, AACENC_GRANULE_LENGTH);
            if (br > 0 && sr > 0 && gl > 0) {
                double avg = (double)br * (double)gl / (double)sr; /* avg bits/frame */
                long avg_i = (long)(avg + 0.5);
                long res_mono = 6144L * 1 - avg_i;
                long res_stereo = 6144L * 2 - avg_i;
                if (res_mono < 0) res_mono = 0;
                if (res_stereo < 0) res_stereo = 0;
                fprintf(stderr, "--- bit reservoir budget (for --vbr-reservoir) ---\n");
                fprintf(stderr, " avg bits/frame        : ~%ld  (bitrate x %u / %u)\n", avg_i, gl, sr);
                fprintf(stderr, " max reservoir (mono)  : ~%ld bits   (6144 - avg)\n", res_mono);
                fprintf(stderr, " max reservoir (stereo): ~%ld bits   (2*6144 - avg)\n", res_stereo);
                fprintf(stderr, "                         (set --vbr-reservoir at or below the ceiling for your channel count)\n");
            }
        }
        /* Franken overrides actually applied this run (kept last so it is obvious
         * which knobs deviate from stock FDK). Only non-default ones are shown. */
        {
            int any = 0;
            #define FRV_HDR() do { if(!any){fprintf(stderr,"--- franken overrides applied ---\n");any=1;} } while(0)
            if (params->fr_ms_precision >= 0)   { FRV_HDR(); fprintf(stderr, " ms-precision          : %d Q8 (>256 = shallower MS holes)\n", params->fr_ms_precision); }
            if (params->fr_ms_bias >= 0)        { FRV_HDR(); fprintf(stderr, " ms-bias               : %d Q8\n", params->fr_ms_bias); }
            if (params->fr_ms_band_lo >= 0 || params->fr_ms_band_hi >= 0) { FRV_HDR(); fprintf(stderr, " ms band range         : lo=%d hi=%d\n", params->fr_ms_band_lo, params->fr_ms_band_hi); }
            if (params->fr_spread_mask >= 0)    { FRV_HDR(); fprintf(stderr, " spread-mask           : %d Q8 (<256 = less masking, more detail)\n", params->fr_spread_mask); }
            if (params->fr_ath_scale >= 0)      { FRV_HDR(); fprintf(stderr, " ath-scale             : %d Q8 (<256 = cleaner/more bits)\n", params->fr_ath_scale); }
            if (params->fr_mid_bias >= 0)       { FRV_HDR(); fprintf(stderr, " mid-bias              : %d Q8 (>256 = free bits from mid for side)\n", params->fr_mid_bias); }
            if (params->fr_side_bias != AACENC_FRANKEN_OFF) { FRV_HDR(); fprintf(stderr, " side-bias             : %+.1f dB (+ = more side, - = degrade side, vs mid)\n", params->fr_side_bias/10.0); }
            if (params->fr_side_knee != AACENC_FRANKEN_OFF) { FRV_HDR(); fprintf(stderr, " side-knee             : %+.1f dB (+ = soft fade, - = hard cutoff)\n", params->fr_side_knee/10.0); }
            if (params->fr_mask_slope != AACENC_FRANKEN_OFF) { FRV_HDR(); fprintf(stderr, " mask-slope            : %+.1f dB (+ = more detail in quiet bands, - = starve harder)\n", params->fr_mask_slope/10.0); }
            if (params->fr_minsnr_scale >= 0)   { FRV_HDR(); fprintf(stderr, " minsnr-scale          : %d Q8 (<256 = demand higher SNR, more detail)\n", params->fr_minsnr_scale); }
            if (params->fr_minsnr_clamp_hi >= 0){ FRV_HDR(); fprintf(stderr, " minsnr-clamp-hi       : %d Q8 (scale MAX_SNR ceiling)\n", params->fr_minsnr_clamp_hi); }
            if (params->fr_minsnr_clamp_lo >= 0){ FRV_HDR(); fprintf(stderr, " minsnr-clamp-lo       : %d Q8 (scale MIN_SNR floor)\n", params->fr_minsnr_clamp_lo); }
            if (params->fr_reduce_clamp == 0)   { FRV_HDR(); fprintf(stderr, " reduce-clamp          : off (29 dB threshold-reduction ceiling dropped, CBR)\n"); }
            if (params->fr_is_band_lo >= 0 || params->fr_is_band_hi >= 0) { FRV_HDR(); fprintf(stderr, " is band range         : lo=%d hi=%d (IS allowed only here)\n", params->fr_is_band_lo, params->fr_is_band_hi); }
            if (params->fr_is_force_lo >= 0 || params->fr_is_force_hi >= 0) { FRV_HDR(); fprintf(stderr, " is FORCE range        : lo=%d hi=%d (IS forced, gates bypassed)\n", params->fr_is_force_lo, params->fr_is_force_hi); }
            if (params->fr_sbr_header_period >= 1) { FRV_HDR(); fprintf(stderr, " sbr-header-period     : %d frame(s) (SBR stream-sync rate)\n", params->fr_sbr_header_period); }
            if (params->fr_is_aggression >= 0)  { FRV_HDR(); fprintf(stderr, " is-aggression         : %d/100\n", params->fr_is_aggression); }
            if (params->fr_block_bias >= 0)     { FRV_HDR(); fprintf(stderr, " block-bias            : %d (128=default, >128 more short blocks)\n", params->fr_block_bias); }
            if (params->fr_uncap_bw == 1)       { FRV_HDR(); fprintf(stderr, " uncap-bandwidth       : on (20 kHz core cap lifted)\n"); }
            if (params->fr_unlock_bitrate == 1) { FRV_HDR(); fprintf(stderr, " unlock-bitrate        : on (lower bitrate floor removed)\n"); }
            if (params->fr_force_pns == 1)      { FRV_HDR(); fprintf(stderr, " force-pns             : on (low-bitrate PNS gate bypassed)\n"); }
            if (params->fr_vbr_reservoir >= 0)  { FRV_HDR(); fprintf(stderr, " vbr-reservoir         : %d bits\n", params->fr_vbr_reservoir); }
            if (params->fr_peak_bitrate >= 0)   { FRV_HDR(); fprintf(stderr, " peak-bitrate          : %d bps\n", params->fr_peak_bitrate); }
            if (params->fr_max_bits_frame >= 0) { FRV_HDR(); fprintf(stderr, " max-bits-frame        : %d\n", params->fr_max_bits_frame); }
            if (params->fr_min_bits_frame >= 0) { FRV_HDR(); fprintf(stderr, " min-bits-frame        : %d\n", params->fr_min_bits_frame); }
            if (params->fr_bitres_mode >= 0)    { FRV_HDR(); fprintf(stderr, " bitres-mode           : %d (0 full,1 reduced,2 rigid)\n", params->fr_bitres_mode); }
            if (!any) fprintf(stderr, "--- franken overrides applied: none (stock FDK behaviour) ---\n");
            #undef FRV_HDR
        }
        fprintf(stderr, "=====================================================================\n");
    }
    return 0;
FAIL:
    if (encoder)
        aacEncClose(encoder);
    return -1;
}

int aac_encode_frame(HANDLE_AACENCODER encoder,
                     const pcm_sample_description_t *format,
                     const INT_PCM *input, unsigned iframes,
                     aacenc_frame_t *output)
{
    uint32_t ilen = iframes * format->channels_per_frame;
    AACENC_BufDesc ibdesc = { 0 }, obdesc = { 0 };
    AACENC_InArgs iargs = { 0 };
    AACENC_OutArgs oargs = { 0 };
    void *ibufs[] = { (void*)input };
    void *obufs[1];
    INT ibuf_ids[] = { IN_AUDIO_DATA };
    INT obuf_ids[] = { OUT_BITSTREAM_DATA };
    INT ibuf_sizes[] = { ilen * sizeof(INT_PCM) };
    INT obuf_sizes[1];
    INT ibuf_el_sizes[] = { sizeof(INT_PCM) };
    INT obuf_el_sizes[] = { 1 };
    AACENC_ERROR err;
    unsigned channel_mode, obytes;

    channel_mode = aacEncoder_GetParam(encoder, AACENC_CHANNELMODE);
    obytes = 6144 / 8 * channel_mode;
    if (!output->data || output->capacity < obytes) {
        uint8_t *p = realloc(output->data, obytes);
        if (!p) return -1;
        output->capacity = obytes;
        output->data = p;
    }
    obufs[0] = output->data;
    obuf_sizes[0] = obytes;

    iargs.numInSamples = ilen ? ilen : -1; /* -1 for signaling EOF */
    ibdesc.numBufs = 1;
    ibdesc.bufs = ibufs;
    ibdesc.bufferIdentifiers = ibuf_ids;
    ibdesc.bufSizes = ibuf_sizes;
    ibdesc.bufElSizes = ibuf_el_sizes;
    obdesc.numBufs = 1;
    obdesc.bufs = obufs;
    obdesc.bufferIdentifiers = obuf_ids;
    obdesc.bufSizes = obuf_sizes;
    obdesc.bufElSizes = obuf_el_sizes;

    err = aacEncEncode(encoder, &ibdesc, &obdesc, &iargs, &oargs);
    if (err != AACENC_ENCODE_EOF && err != AACENC_OK) {
        fprintf(stderr, "ERROR: aacEncEncode() failed\n");
        return -1;
    }
    output->size = oargs.numOutBytes;
    return oargs.numInSamples / format->channels_per_frame;
}
