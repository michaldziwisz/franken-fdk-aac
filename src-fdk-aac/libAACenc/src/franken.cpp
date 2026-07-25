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

  g_franken.isBandLo = -1;
  g_franken.isBandHi = -1;
  g_franken.isForceLo = -1;
  g_franken.isForceHi = -1;

  g_franken.minSnrScaleQ8 = -1;
  g_franken.minSnrClampHiQ8 = -1;
  g_franken.minSnrClampLoQ8 = -1;
  g_franken.reduceClamp = -1;

  g_franken.midBiasQ8 = -1;
  g_franken.sideBiasDbX10 = FRANKEN_OFF;
  g_franken.sideKneeDbX10 = FRANKEN_OFF;
  g_franken.msaSlopeDbX10 = FRANKEN_OFF;

  g_franken.sbrHeaderPeriod = -1;
  g_franken.effSbrHeaderPeriod = -1;

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

/* Ensure defaults are set even if the frontend never calls the reset
 * (e.g. library used directly). Uses a constructor with lowest overhead. */
namespace {
struct FrankenInit {
  FrankenInit() { frankenResetDefaults(); }
} g_frankenInit;
}

