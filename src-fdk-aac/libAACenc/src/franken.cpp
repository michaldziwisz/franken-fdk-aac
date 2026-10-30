/* franken.cpp - definition + defaults for Frankenstein debug overrides. */
#include "franken.h"

FRANKEN_CFG g_franken;

extern "C" void frankenResetDefaults(void) {
  g_franken.forceMsMask = -1;
  g_franken.msMaxBands = -1;
  g_franken.forceIs = -1;
  g_franken.isMaxBands = -1;
  g_franken.isMinSfbs = -1;
  g_franken.isCorrThreshQ8 = -1;
  g_franken.isLRRatioQ8 = -1;

  g_franken.coreCutoff = 0;

  g_franken.sbrStartFreq = -1;
  g_franken.sbrStopFreq = -1;
  g_franken.sbrFreqScale = -1;
  g_franken.sbrAlterScale = -1;
  g_franken.sbrNoiseBands = -1;
  g_franken.sbrAmpRes = -1;
  g_franken.sbrDataExtra = -1;

  g_franken.psEnable = -1;
  g_franken.psIidQuant = -1;

  g_franken.tnsMask = -1;
  g_franken.tnsMaxOrder = -1;
  g_franken.usePns = -1;
  g_franken.pnsStartFreq = -1;
  g_franken.afterburner = -1;

  g_franken.athScaleQ8 = -1;

  g_franken.blockBias = -1;

  g_franken.verbose = 0;

  g_franken.vbrReservoir = -1;
  g_franken.maxBitsFrame = -1;
  g_franken.minBitsFrame = -1;
  g_franken.bitresMode = -1;
  g_franken.msBiasQ8 = -1;

  g_franken.uncapBandwidth = -1;
  g_franken.isAggression = -1;
  g_franken.forcePns = -1;
  g_franken.unlockBitrate = -1;
  g_franken.speechConfig = -1;
  g_franken.spreadMaskQ8 = -1;

  g_franken.msBandLo = -1;
  g_franken.msBandHi = -1;
  g_franken.msPrecisionQ8 = -1;

  g_franken.sbrNumEnv = -1;
  g_franken.sbrFreqResFixfix = -1;
  g_franken.sbrStereoMode = -1;
  g_franken.sbrInvfMode = -1;
  g_franken.sbrNoiseFloorOffset = -128;

  g_franken.psIcc = -1;
  g_franken.psIccMode = -1;
  g_franken.sbrTranPeakX100 = -1;
  g_franken.sbrTranQuietX100 = -1;
  g_franken.sbrTranDomX100 = -1;
  g_franken.sbrTranThrX100 = -1;
  g_franken.sbrTranSplitX100 = -1;
  g_franken.sbrMhToneX100 = -1;
  g_franken.sbrMhDiffX100 = -1;
  g_franken.sbrMhDecayOrigX100 = -1;
  g_franken.sbrMhDecayDiffX100 = -1;
  g_franken.sbrMhSfmSbrX100 = -1;
  g_franken.sbrMhSfmOrigX100 = -1;
  g_franken.sbrMhMaxComp = -1;
  g_franken.sbrMhDeltaTime = -1;
  g_franken.sbrNoiseMaxLevel = -128;
  g_franken.sbrInvfMin = -1;
  g_franken.sbrInvfMax = -1;
  g_franken.sbrInvfBias = FRANKEN_OFF;
  g_franken.sbrInvfTable = -1;
  g_franken.sbrInvfHystX100 = -1;
  g_franken.sbrInvfSbrThrX100 = FRANKEN_OFF;
  g_franken.sbrInvfOrigThrX100 = FRANKEN_OFF;
  g_franken.sbrInvfSmooth = -1;
  g_franken.sbrNfWeightX1000 = -1;
  g_franken.sbrNfWeightFrom = -1;
  g_franken.sbrNfSmooth = -1;
  g_franken.sbrSoftness = -1;
  g_franken.preEchoRiseDbX1000 = FRANKEN_OFF;
  g_franken.preEchoDepthDbX1000 = FRANKEN_OFF;
  g_franken.attackDecayX1000 = FRANKEN_OFF;
  g_franken.attackFloorDbX1000 = FRANKEN_OFF;
  g_franken.tnsThreshX1000 = FRANKEN_OFF;
  g_franken.tnsStartHz = -1;
  g_franken.windowShape = -1;
  g_franken.shortGroups = -1;
  g_franken.avoidHoles = -1;
  g_franken.holeStartLong = -1;
  g_franken.holeStartShort = -1;
  g_franken.holeSpreadDbX1000 = FRANKEN_OFF;
  g_franken.holeLadder = -1;
  g_franken.bandKneeDbX1000 = FRANKEN_OFF;
  g_franken.quantRoundX1000 = -1;
  g_franken.pnsAboveHz = -1;
  g_franken.thrHold = -1;
  g_franken.scfRange = -1;
  g_franken.coverage = FRANKEN_OFF;
  g_franken.enfacPatch = -1;
  g_franken.psBands = -1;
  g_franken.psEnvelopes = -1;
  g_franken.psEnvReduce = -1;
  g_franken.psIpd = -1;
  g_franken.psOpd = -1;
  g_franken.psNoEnvSkip = -1;
  g_franken.effPsBands = -1;
  g_franken.effPsEnvelopes = -1;

  g_franken.isBandLo = -1;
  g_franken.isBandHi = -1;
  g_franken.isForceLo = -1;
  g_franken.isForceHi = -1;

  g_franken.minSnrScaleQ8 = -1;
  g_franken.minSnrClampHiQ8 = -1;
  g_franken.minSnrClampLoQ8 = -1;
  g_franken.reduceClamp = -1;

  g_franken.midBiasQ8 = -1;
  g_franken.sideBiasDbX1000 = FRANKEN_OFF;
  g_franken.sideKneeDbX1000 = FRANKEN_OFF;
  g_franken.msaSlopeDbX1000 = FRANKEN_OFF;

  g_franken.sbrHeaderPeriod = -1;
  g_franken.effSbrHeaderPeriod = -1;
  g_franken.sbrHeaderTimeMs = -1;
  g_franken.effTpConfigPeriod = -1;

  g_franken.pnsGainX100 = -1;
  g_franken.pnsTonalityX100 = -1;
  g_franken.pnsRefPowerX100 = -1;
  g_franken.pnsGapFillX100 = -1;
  g_franken.pnsMinWidth = -1;
  g_franken.effBandwidthHz = -1;

  g_franken.effSbrActive = 0;
  g_franken.effSbrStart = -1;
  g_franken.effSbrStop = -1;
  g_franken.effSbrFreqScale = -1;
  g_franken.effSbrNoiseBands = -1;
  g_franken.effSbrAmpRes = -1;
  g_franken.effSbrStopHz = -1;
  g_franken.effMaxSfb = -1;
  g_franken.effTnsMaxOrder = -1;
  g_franken.effTnsMask = -1;
  g_franken.effPnsStartHz = -1;
  /* IS decision thresholds are compile-time constants in FDK (intensity.cpp);
   * seed their real defaults so --verbose shows them even before the first
   * frame runs initIsParams(). Overridden live if the user sets them. */
  g_franken.effIsMinSfbs = (g_franken.isMinSfbs >= 0) ? g_franken.isMinSfbs : 6;
  g_franken.effIsCorrQ8 = (g_franken.isCorrThreshQ8 >= 0) ? g_franken.isCorrThreshQ8 : 243; /* 0.95 */
  g_franken.effIsLrRatioQ8 = (g_franken.isLRRatioQ8 >= 0) ? g_franken.isLRRatioQ8 : 179; /* 0.70 */
}

/* ---- SBR estimator macro (--sbr-softness 0..7) ----
 * One knob between a "hard / metallic" high band (little inverse filtering,
 * no extra noise) and a "very soft" one (more whitening and more fill noise),
 * built ONLY from the estimator knobs, so the invf decision stays dynamic at
 * every level. Level 2 = stock music estimators, level 4 = stock speech
 * estimators (decision table + noise weight; the speech crossover and the
 * other --speech changes are NOT part of it).
 * Built from the decision table, an integer bias and the noise weighting:
 * those keep the frame-to-frame stability of the stock estimator. Shifting
 * the tonality borders instead was measured to make the decision flicker
 * (0.004 -> 0.2..0.29 mode changes per band and frame), so the borders are
 * left to the explicit expert knobs.
 * Columns: decision table, bias, noise weight x1000, lowest invf level that
 * gets the weighted noise (3 = stock, HIGH only), noise ceiling (0 = from the
 * tuning table, else 6/3/-3 as --sbr-noise-max).
 * Measured: with the tuning-table ceiling (-3 = 0.125 at 24..48 kHz) the
 * estimated fill noise is clipped in ~95% of the bands, so the weight alone
 * changed nothing there (levels 1/2 and 3..5 gave identical streams on most
 * material). Levels 5..7 therefore also lift the ceiling, and levels 1 and 3
 * use an invf bound (1: never HIGH, 3: at least LOW). The scale is monotonic
 * (mean invf level and HF flatness grow 0 -> 7 on every test sample), but two
 * neighbours can still give the same stream where the material never reaches
 * the bound they differ in (e.g. 1 = 2 when the music estimator never picks
 * HIGH). */
static const struct {
  signed char table, bias;
  short nfWeight;
  signed char nfFrom;
  signed char noiseMax;
  signed char invfMin, invfMax; /* -1 = no bound */
} kSoftness[8] = {
    {0, -1, 0, 3, 0, -1, -1},    /* 0 harder than music: one step less whitening */
    {0, 0, 0, 3, 0, -1, 2},      /* 1 music table, never HIGH whitening */
    {0, 0, 250, 3, 0, -1, -1},   /* 2 = stock music estimators */
    {0, 0, 250, 3, 0, 1, -1},    /* 3 music table, at least LOW whitening */
    {1, 0, 1000, 3, 0, -1, -1},  /* 4 = stock speech estimators */
    {1, 0, 1000, 2, 3, -1, -1},  /* 5 weighted noise also in MID bands, ceiling 0.5 */
    {1, 1, 1000, 2, 3, -1, -1},  /* 6 one step more whitening */
    {1, 1, 1000, 1, 6, -1, -1},  /* 7 very soft, ceiling 1.0, still dynamic */
};

/* Noise ceiling: explicit --sbr-noise-max wins; else the macro; 0 = table. */
extern "C" int frankenEffNoiseMax(void) {
  if (g_franken.sbrNoiseMaxLevel == 6 || g_franken.sbrNoiseMaxLevel == 3 ||
      g_franken.sbrNoiseMaxLevel == -3)
    return g_franken.sbrNoiseMaxLevel;
  if (g_franken.sbrSoftness >= 0 && g_franken.sbrSoftness <= 7)
    return kSoftness[g_franken.sbrSoftness].noiseMax;
  return 0;
}

static int softLevel(void) {
  return (g_franken.sbrSoftness >= 0 && g_franken.sbrSoftness <= 7)
             ? g_franken.sbrSoftness
             : -1;
}

extern "C" int frankenEffInvfTable(int stockSpeech) {
  if (g_franken.sbrInvfTable == 0 || g_franken.sbrInvfTable == 1)
    return g_franken.sbrInvfTable;
  if (softLevel() >= 0) return kSoftness[softLevel()].table;
  return stockSpeech ? 1 : 0;
}

extern "C" int frankenEffInvfBias(void) {
  if (g_franken.sbrInvfBias != FRANKEN_OFF) return g_franken.sbrInvfBias;
  if (softLevel() >= 0) return kSoftness[softLevel()].bias;
  return 0;
}

extern "C" int frankenEffInvfMin(void) {
  if (g_franken.sbrInvfMin >= 0 && g_franken.sbrInvfMin <= 3)
    return g_franken.sbrInvfMin;
  if (softLevel() >= 0) return kSoftness[softLevel()].invfMin;
  return -1;
}

extern "C" int frankenEffInvfMax(void) {
  if (g_franken.sbrInvfMax >= 0 && g_franken.sbrInvfMax <= 3)
    return g_franken.sbrInvfMax;
  if (softLevel() >= 0) return kSoftness[softLevel()].invfMax;
  return -1;
}

extern "C" int frankenEffNfWeightX1000(int stockSpeech) {
  if (g_franken.sbrNfWeightX1000 >= 0) return g_franken.sbrNfWeightX1000;
  if (softLevel() >= 0) {
    const int w = kSoftness[softLevel()].nfWeight;
    /* -1 where the macro equals the stock value of the current mode, so
     * --sbr-softness 2 (music) / 4 (with --speech) stay bit-identical */
    if (w == (stockSpeech ? 1000 : 250)) return -1;
    return w;
  }
  return -1;
}

extern "C" int frankenEffNfWeightFrom(void) {
  if (g_franken.sbrNfWeightFrom >= 1 && g_franken.sbrNfWeightFrom <= 3)
    return g_franken.sbrNfWeightFrom;
  if (softLevel() >= 0 && kSoftness[softLevel()].nfFrom != 3)
    return kSoftness[softLevel()].nfFrom;
  return -1;
}

/* Border shifts: explicit expert knobs only (see note above). */
extern "C" int frankenEffInvfSbrThrX100(void) {
  return g_franken.sbrInvfSbrThrX100;
}

extern "C" int frankenEffInvfOrigThrX100(void) {
  return g_franken.sbrInvfOrigThrX100;
}

/* ---- Coverage macro (--coverage -5..5) ----
 * One axis for a fixed bitrate: + spends the bits on coding EVERY band coarsely
 * (no holes, everything filled, more granular), - spends them on coding fewer
 * bands cleanly (marginal bands dropped, dead-zone rounding, holes allowed).
 * Columns: band knee dB*1000 (FRANKEN_OFF = none), quantizer rounding x1000
 * (-1 = stock), hole ladder (-1 = stock), avoid-holes (-1 = stock), first long
 * SFB where holes may be cut (-1 = stock), noise substitution from this
 * frequency up (-1 = off). Index = level + 5.
 * Measured (LC 32 kbps, 17 kHz, dense test signal): the + side saturates at
 * about 8..10 % zeroed bands with the knobs of +2/+3; a larger knee is
 * counterproductive (rescued coarse bands cost bits, the rate loop then cuts
 * elsewhere), so +4/+5 add noise substitution instead. The - side drops
 * marginal bands; the gain in cleanliness of what is left is about 1 dB. */
static const struct {
  int kneeX1000;
  short roundX1000;
  signed char ladder, avoidHoles, holeStart;
  int pnsAboveHz;
} kCoverage[11] = {
    {-10000, -1, -1, 0, -1, -1},          /* -5 */
    {-8000, -1, -1, 0, -1, -1},           /* -4 */
    {-6000, -1, -1, 0, -1, -1},           /* -3 */
    {-4000, -1, -1, -1, -1, -1},          /* -2 */
    {-2000, -1, -1, -1, -1, -1},          /* -1 */
    {FRANKEN_OFF, -1, -1, -1, -1, -1},    /*  0 = stock */
    {3000, 420, 4, 1, -1, -1},            /* +1 */
    {6000, 430, 2, 1, -1, -1},            /* +2 */
    {6000, 440, 1, 1, -1, -1},            /* +3 */
    {6000, 440, 1, 1, -1, 11000},         /* +4: and noise from 11 kHz up */
    {6000, 450, 1, 1, -1, 7000},          /* +5: and noise from 7 kHz up */
};

static int covIdx(void) {
  if (g_franken.coverage == FRANKEN_OFF) return -1;
  if (g_franken.coverage < -5 || g_franken.coverage > 5) return -1;
  return g_franken.coverage + 5;
}

extern "C" int frankenEffBandKnee(void) {
  if (g_franken.bandKneeDbX1000 != FRANKEN_OFF) return g_franken.bandKneeDbX1000;
  if (covIdx() >= 0) return kCoverage[covIdx()].kneeX1000;
  return FRANKEN_OFF;
}
extern "C" int frankenEffQuantRound(void) {
  if (g_franken.quantRoundX1000 >= 0) return g_franken.quantRoundX1000;
  if (covIdx() >= 0) return kCoverage[covIdx()].roundX1000;
  return -1;
}
extern "C" int frankenEffHoleLadder(void) {
  if (g_franken.holeLadder >= 1) return g_franken.holeLadder;
  if (covIdx() >= 0) return kCoverage[covIdx()].ladder;
  return -1;
}
extern "C" int frankenEffAvoidHoles(void) {
  if (g_franken.avoidHoles == 0 || g_franken.avoidHoles == 1) return g_franken.avoidHoles;
  if (covIdx() >= 0) return kCoverage[covIdx()].avoidHoles;
  return -1;
}
extern "C" int frankenEffHoleStartLong(void) {
  if (g_franken.holeStartLong >= 0) return g_franken.holeStartLong;
  if (covIdx() >= 0) return kCoverage[covIdx()].holeStart;
  return -1;
}

extern "C" int frankenEffPnsAbove(void) {
  if (g_franken.pnsAboveHz > 0) return g_franken.pnsAboveHz;
  if (covIdx() >= 0) return kCoverage[covIdx()].pnsAboveHz;
  return -1;
}

/* Ensure defaults are set even if the frontend never calls the reset
 * (e.g. library used directly). Uses a constructor with lowest overhead. */
namespace {
struct FrankenInit {
  FrankenInit() { frankenResetDefaults(); }
} g_frankenInit;
}

