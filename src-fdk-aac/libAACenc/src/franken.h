/* franken.h - Frankenstein debug overrides for FDK-AAC encoder.
 *
 * A single process-global control block that internal encoder modules
 * consult to override FDK's normally-hardcoded algorithmic decisions.
 *
 * LABORATORY / DEBUG USE ONLY. Not thread-safe across multiple concurrent
 * encoder instances in one process (fine for the CLI, which encodes one
 * file per process). Every field defaults to a sentinel meaning
 * "keep FDK default behaviour", so an unpatched call path is byte-identical
 * to the stock encoder.
 */
#ifndef FRANKEN_H
#define FRANKEN_H

typedef struct FRANKEN_CFG {
  /* ---- 1. Joint stereo / MS / IS ---- */
  int forceMsMask;   /* -1 auto (FDK), 0 force MS off (all bands), 1 force MS on (all bands) */
  int msMaxBands;    /* -1 = no limit; else max SFB index allowed to use MS */
  int forceIs;       /* -1 auto, 0 disable IS entirely, 1 leave IS decisions to FDK but allow band cap */
  int isMaxBands;    /* -1 = no limit; else max number of SFBs allowed to use intensity */
  int isMinSfbs;     /* -1 default(6); min contiguous SFBs for an IS region (higher = IS rarer) */
  int isCorrThreshQ8;/* -1 default(0.95); L/R correlation threshold in Q8 (243=0.95). Lower = IS more eager */
  int isLRRatioQ8;   /* -1 default(0.7); panning L/R ratio threshold in Q8 (179=0.7) */

  /* ---- 2. Core AAC cutoff even when SBR active ---- */
  int coreCutoff;    /* 0 = FDK default; else forced core bandwidth in Hz, protected from SBR override */

  /* ---- 3. SBR data density / accuracy ---- */
  int sbrStartFreq;  /* -1 default; else bs_start_freq table index */
  int sbrStopFreq;   /* -1 default; else bs_stop_freq table index */
  int sbrFreqScale;  /* -1 default; else 0..3 */
  int sbrAlterScale; /* -1 default; else 0/1 */
  int sbrNoiseBands; /* -1 default; else noise band count */
  int sbrAmpRes;     /* -1 default; else 0 (1.5 dB) or 1 (3.0 dB) */
  int sbrDataExtra;  /* -1 default; else 0/1 write extra SBR header data */

  /* ---- 4. Parametric Stereo internals ---- */
  int psEnable;      /* -1 auto, 0 force PS parameter sending off, 1 force on */
  int psIidQuant;    /* -1 default; 0 coarse, 1 fine IID quantisation */

  /* ---- 5. TNS / PNS / afterburner ---- */
  int tnsMask;       /* -1 default (0xF); else 0..0xF enable mask per block type */
  int tnsMaxOrder;   /* -1 default; else 1..12 clamp on filter order */
  int usePns;        /* -1 default; 0/1 */
  int pnsStartFreq;  /* -1 default; else PNS start frequency in Hz */
  int afterburner;   /* -1 default; 0/1 */

  /* ---- 6. ATH / masking ---- */
  int athScaleQ8;    /* -1 default; else masking-threshold scale in Q8 (256 = x1.0). >256 raises
                        thresholds (more aggressive noise), <256 lowers (cleaner). */

  /* ---- 7. Block-switch bias ---- */
  int blockBias;     /* -1 default; else 0..255 scale on attack ratio. 128 = FDK
                        default, >128 = more short blocks, <128 = more long, 0 = long-only. */

  /* ---- 8. Verbose ---- */
  int verbose;       /* 0/1 (frontend-side, kept here for completeness) */

  /* ---- 9. Quasi-constrained VBR (CBR engine + wider reservoir/caps) ---- */
  int vbrReservoir;  /* -1/0 default; else bit-reservoir size in bits (clamped to 6144*ch-avg) */
  int maxBitsFrame;  /* -1 default; else hard max bits per frame (<= 6144*ch) */
  int minBitsFrame;  /* -1 default; else hard min bits per frame (>= 0) */
  int bitresMode;    /* -1 default; 0 full, 1 reduced, 2 disabled reservoir */

  /* ---- 10. MS decision bias ---- */
  int msBiasQ8;      /* -1 default; else bias added to MS gain decision, Q8 (128=+0.5). >0 = MS eager */

  /* ---- 11. Uncap / gate-bypass (audiophile + extreme) ---- */
  int uncapBandwidth; /* -1/0 default; 1 = lift the fMin(20000,sr/2) core-cutoff cap */
  int isAggression;   /* -1 default; 0 = FDK default, 1..100 = progressively more aggressive IS */
  int forcePns;       /* -1/0 default; 1 = bypass low-bitrate PNS gate (lookUpPnsUse) */
  int unlockBitrate;  /* -1/0 default; 1 = remove lower bitrate floor (extreme low rates) */
  int speechConfig;   /* -1/0 default; 1 = enable SBR speech tuning (useSpeechConfig) */
  int spreadMaskQ8;   /* -1 default; else scale psy spreading (mask spread) Q8. <256 = less
                         inter-band masking = more detail preserved (high-bitrate). */

  /* ---- 12. MS band range + precision (this batch) ---- */
  int msBandLo;       /* -1 default; else lowest SFB index eligible for MS */
  int msBandHi;       /* -1 default; else highest SFB index eligible for MS (inclusive) */
  int msPrecisionQ8;  /* -1 default; else lower MS-band threshold by this Q8 factor (>256 = deeper cut of
                         threshold = shallower quant holes = more bits). ld64 additive offset. */

  /* ---- 13. SBR envelope grid / stereo / inverse filtering (this batch) ---- */
  int sbrNumEnv;      /* -1 default; else static envelopes per FIXFIX frame {1,2,4,8} */
  int sbrFreqResFixfix; /* -1 default; else freq resolution of FIXFIX envelopes (0 low, 1 high) */
  int sbrStereoMode;  /* -1 default; else 0 mono,1 LR,2 coupling,3 switch-LRC */
  int sbrInvfMode;    /* -1 default; else forced inverse-filtering level 0 off,1 low,2 mid,3 high */
  int sbrNoiseFloorOffset; /* -128 default (unset); else SBR noise floor offset (small int) */

  /* ---- 14. Parametric stereo ICC (this batch) ---- */
  int psIcc;          /* -1 default; 0 force ICC off, 1 force ICC on */
  int psIccMode;      /* -1 default; else ICC rotation mode 0 ROT_A, 1 ROT_B */

  /* ---- 15. Intensity stereo band range + force (this batch) ---- */
  int isBandLo;       /* -1 default; else lowest SFB index eligible for IS */
  int isBandHi;       /* -1 default; else highest SFB index eligible for IS (inclusive) */
  int isForceLo;      /* -1 default; else force IS ON from this SFB (inclusive) */
  int isForceHi;      /* -1 default; else force IS ON up to this SFB (inclusive).
                         When isForceLo/Hi set, IS is forced regardless of the
                         correlation / min-sfbs / loudness gates in [lo,hi]. */

  /* ---- 16. MusePack-style masking knobs (opt-in, safe variant) ---- */
  int minSnrScaleQ8;  /* -1 default; else scale required per-band min-SNR, Q8 (256=x1.0).
                         <256 = demand HIGHER coding SNR (more detail/bits),
                         >256 = allow LOWER SNR (coarser). Applied as ld64 offset
                         AFTER FDK's own MIN/MAX_SNR clamps, so it can reach beyond
                         the stock -1..-25 dB window. */
  int minSnrClampHiQ8;/* -1 default; else scale FDK's MAX_SNR ceiling (0.8), Q8. */
  int minSnrClampLoQ8;/* -1 default; else scale FDK's MIN_SNR floor (0.003), Q8. */
  int reduceClamp;    /* -1 default(on); 0 = drop the "29 dB Ratio" threshold-reduction
                         clamp in adj_thr, letting thresholds be pushed deeper. */

  /* ---- 17. Stereo bit-split precision ---- */
  int midBiasQ8;      /* -1 default; else RAISE the MID (L+R) channel masking
                         threshold by this Q8 factor after the MS butterfly.
                         >256 = deliberately free bits from mid for side. */

  /* ---- 18. SBR header period (streaming SBR sync) ---- */
  int sbrHeaderPeriod;/* -1 default(FDK, ~10 frames); else frames between SBR headers.
                         1 = SBR config in every frame => decoder locks SBR almost
                         instantly when tuning into an Icecast/Shoutcast stream;
                         higher = longer core-only period before SBR kicks in. */
  int effSbrHeaderPeriod; /* read-back: effective NrSendHeaderData (-1 n/a) */

  /* ---- 19. PNS shaping (loudness / detection width of the fabricated noise) ---- */
  int pnsGainX100;    /* -1 default(off). Scales the CODED PNS noise energy (the
                         loudness of the noise the decoder fabricates). value*100,
                         1.00 -> 100 = unchanged. >1.0 = louder-than-original noise,
                         <1.0 = quieter. Applied as an ld64 offset to noiseNrg. */
  int pnsTonalityX100;/* -1 default. Scales refTonality detection threshold, value*100
                         (1.00->100). Higher = more (also tonal-ish) bands qualify as
                         PNS => wider noise substitution. */
  int pnsRefPowerX100;/* -1 default. Scales refPower detection threshold, value*100. */
  int pnsGapFillX100; /* -1 default. Scales gapFillThr, value*100. */
  int pnsMinWidth;    /* -1 default; else min SFB width for PNS (raw int). */

  /* ---- Read-back: effective values chosen by the encoder (for --verbose) ----
   * Populated by the encoder during init regardless of overrides. -1 = unknown. */
  int effSbrActive;
  int effSbrStart;
  int effSbrStop;
  int effSbrFreqScale;
  int effSbrNoiseBands;
  int effSbrAmpRes;
  int effSbrStopHz;   /* final AAC+SBR bandwidth in Hz (from stop index), -1 n/a */
  int effBandwidthHz; /* effective core cutoff in Hz, anchored to the SFB boundary
                         (what -w / --core-cutoff really became), -1 n/a */
  int effMaxSfb;      /* number of active SFBs (ceiling for MS/IS band counts) */
  int effTnsMaxOrder; /* effective TNS max filter order (long window) */
  int effTnsMask;     /* effective TNS enable mask */
  int effPnsStartHz;  /* effective PNS start frequency in Hz (-1 if PNS off) */
  int effIsMinSfbs;   /* effective IS min contiguous SFBs */
  int effIsCorrQ8;    /* effective IS correlation threshold, Q8 */
  int effIsLrRatioQ8; /* effective IS L/R ratio threshold, Q8 */
} FRANKEN_CFG;

#ifdef __cplusplus
extern "C" {
#endif
extern FRANKEN_CFG g_franken;
void frankenResetDefaults(void);
#ifdef __cplusplus
}
#endif

#endif /* FRANKEN_H */
