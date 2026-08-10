/* -----------------------------------------------------------------------------
Software License for The Fraunhofer FDK AAC Codec Library for Android

© Copyright  1995 - 2018 Fraunhofer-Gesellschaft zur Förderung der angewandten
Forschung e.V. All rights reserved.

 1.    INTRODUCTION
The Fraunhofer FDK AAC Codec Library for Android ("FDK AAC Codec") is software
that implements the MPEG Advanced Audio Coding ("AAC") encoding and decoding
scheme for digital audio. This FDK AAC Codec software is intended to be used on
a wide variety of Android devices.

AAC's HE-AAC and HE-AAC v2 versions are regarded as today's most efficient
general perceptual audio codecs. AAC-ELD is considered the best-performing
full-bandwidth communications codec by independent studies and is widely
deployed. AAC has been standardized by ISO and IEC as part of the MPEG
specifications.

Patent licenses for necessary patent claims for the FDK AAC Codec (including
those of Fraunhofer) may be obtained through Via Licensing
(www.vialicensing.com) or through the respective patent owners individually for
the purpose of encoding or decoding bit streams in products that are compliant
with the ISO/IEC MPEG audio standards. Please note that most manufacturers of
Android devices already license these patent claims through Via Licensing or
directly from the patent owners, and therefore FDK AAC Codec software may
already be covered under those patent licenses when it is used for those
licensed purposes only.

Commercially-licensed AAC software libraries, including floating-point versions
with enhanced sound quality, are also available from Fraunhofer. Users are
encouraged to check the Fraunhofer website for additional applications
information and documentation.

2.    COPYRIGHT LICENSE

Redistribution and use in source and binary forms, with or without modification,
are permitted without payment of copyright license fees provided that you
satisfy the following conditions:

You must retain the complete text of this software license in redistributions of
the FDK AAC Codec or your modifications thereto in source code form.

You must retain the complete text of this software license in the documentation
and/or other materials provided with redistributions of the FDK AAC Codec or
your modifications thereto in binary form. You must make available free of
charge copies of the complete source code of the FDK AAC Codec and your
modifications thereto to recipients of copies in binary form.

The name of Fraunhofer may not be used to endorse or promote products derived
from this library without prior written permission.

You may not charge copyright license fees for anyone to use, copy or distribute
the FDK AAC Codec software or your modifications thereto.

Your modified versions of the FDK AAC Codec must carry prominent notices stating
that you changed the software and the date of any change. For modified versions
of the FDK AAC Codec, the term "Fraunhofer FDK AAC Codec Library for Android"
must be replaced by the term "Third-Party Modified Version of the Fraunhofer FDK
AAC Codec Library for Android."

3.    NO PATENT LICENSE

NO EXPRESS OR IMPLIED LICENSES TO ANY PATENT CLAIMS, including without
limitation the patents of Fraunhofer, ARE GRANTED BY THIS SOFTWARE LICENSE.
Fraunhofer provides no warranty of patent non-infringement with respect to this
software.

You may use this FDK AAC Codec software or modifications thereto only for
purposes that are authorized by appropriate patent licenses.

4.    DISCLAIMER

This FDK AAC Codec software is provided by Fraunhofer on behalf of the copyright
holders and contributors "AS IS" and WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
including but not limited to the implied warranties of merchantability and
fitness for a particular purpose. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE for any direct, indirect, incidental, special, exemplary,
or consequential damages, including but not limited to procurement of substitute
goods or services; loss of use, data, or profits, or business interruption,
however caused and on any theory of liability, whether in contract, strict
liability, or tort (including negligence), arising in any way out of the use of
this software, even if advised of the possibility of such damage.

5.    CONTACT INFORMATION

Fraunhofer Institute for Integrated Circuits IIS
Attention: Audio and Multimedia Departments - FDK AAC LL
Am Wolfsmantel 33
91058 Erlangen, Germany

www.iis.fraunhofer.de/amm
amm-info@iis.fraunhofer.de
----------------------------------------------------------------------------- */

/**************************** SBR encoder library ******************************

   Author(s):   M. Neuendorf, N. Rettelbach, M. Multrus

   Description: PS parameter extraction, encoding

*******************************************************************************/

/*!
  \file
  \brief  PS parameter extraction, encoding functions $Revision: 96441 $
*/

#include "ps_main.h"
#include "ps_encode.h"
#include "../../libAACenc/src/franken.h"
#include "FDK_trigFcts.h"
#include "qmf.h"
#include "sbr_misc.h"
#include "sbrenc_ram.h"

#include "genericStds.h"

inline void FDKsbrEnc_addFIXP_DBL(const FIXP_DBL *X, const FIXP_DBL *Y,
                                  FIXP_DBL *Z, INT n) {
  for (INT i = 0; i < n; i++) Z[i] = (X[i] >> 1) + (Y[i] >> 1);
}

#define LOG10_2_10 3.01029995664f /* 10.0f*log10(2.f) */

static const INT
    iidGroupBordersLoRes[QMF_GROUPS_LO_RES + SUBQMF_GROUPS_LO_RES + 1] = {
        0,  1,  2,  3,  4,  5, /* 6 subqmf subbands - 0th qmf subband */
        6,  7,                 /* 2 subqmf subbands - 1st qmf subband */
        8,  9,                 /* 2 subqmf subbands - 2nd qmf subband */
        10, 11, 12, 13, 14, 15, 16, 18, 21, 25, 30, 42, 71};

static const UCHAR
    iidGroupWidthLdLoRes[QMF_GROUPS_LO_RES + SUBQMF_GROUPS_LO_RES] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 3, 4, 5};

static const INT subband2parameter20[QMF_GROUPS_LO_RES + SUBQMF_GROUPS_LO_RES] =
    {1, 0, 0,  1,  2,  3, /* 6 subqmf subbands - 0th qmf subband */
     4, 5,                /* 2 subqmf subbands - 1st qmf subband */
     6, 7,                /* 2 subqmf subbands - 2nd qmf subband */
     8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

typedef enum {
  MAX_TIME_DIFF_FRAMES = 20,
  MAX_PS_NOHEADER_CNT = 10,
  MAX_NOENV_CNT = 10,
  DO_NOT_USE_THIS_MODE = 0x7FFFFF
} __PS_CONSTANTS;

static const FIXP_DBL iidQuant_fx[15] = {
    (FIXP_DBL)0xce000000, (FIXP_DBL)0xdc000000, (FIXP_DBL)0xe4000000,
    (FIXP_DBL)0xec000000, (FIXP_DBL)0xf2000000, (FIXP_DBL)0xf8000000,
    (FIXP_DBL)0xfc000000, (FIXP_DBL)0x00000000, (FIXP_DBL)0x04000000,
    (FIXP_DBL)0x08000000, (FIXP_DBL)0x0e000000, (FIXP_DBL)0x14000000,
    (FIXP_DBL)0x1c000000, (FIXP_DBL)0x24000000, (FIXP_DBL)0x32000000};

static const FIXP_DBL iidQuantFine_fx[31] = {
    (FIXP_DBL)0x9c000001, (FIXP_DBL)0xa6000001, (FIXP_DBL)0xb0000001,
    (FIXP_DBL)0xba000001, (FIXP_DBL)0xc4000000, (FIXP_DBL)0xce000000,
    (FIXP_DBL)0xd4000000, (FIXP_DBL)0xda000000, (FIXP_DBL)0xe0000000,
    (FIXP_DBL)0xe6000000, (FIXP_DBL)0xec000000, (FIXP_DBL)0xf0000000,
    (FIXP_DBL)0xf4000000, (FIXP_DBL)0xf8000000, (FIXP_DBL)0xfc000000,
    (FIXP_DBL)0x00000000, (FIXP_DBL)0x04000000, (FIXP_DBL)0x08000000,
    (FIXP_DBL)0x0c000000, (FIXP_DBL)0x10000000, (FIXP_DBL)0x14000000,
    (FIXP_DBL)0x1a000000, (FIXP_DBL)0x20000000, (FIXP_DBL)0x26000000,
    (FIXP_DBL)0x2c000000, (FIXP_DBL)0x32000000, (FIXP_DBL)0x3c000000,
    (FIXP_DBL)0x45ffffff, (FIXP_DBL)0x4fffffff, (FIXP_DBL)0x59ffffff,
    (FIXP_DBL)0x63ffffff};

static const FIXP_DBL iccQuant[8] = {
    (FIXP_DBL)0x7fffffff, (FIXP_DBL)0x77ef9d7f, (FIXP_DBL)0x6babc97f,
    (FIXP_DBL)0x4ceaf27f, (FIXP_DBL)0x2f0ed3c0, (FIXP_DBL)0x00000000,
    (FIXP_DBL)0xb49ba601, (FIXP_DBL)0x80000000};

static FDK_PSENC_ERROR InitPSData(HANDLE_PS_DATA hPsData) {
  FDK_PSENC_ERROR error = PSENC_OK;

  if (hPsData == NULL) {
    error = PSENC_INVALID_HANDLE;
  } else {
    int i, env;
    FDKmemclear(hPsData, sizeof(PS_DATA));

    for (i = 0; i < PS_MAX_BANDS; i++) {
      hPsData->iidIdxLast[i] = 0;
      hPsData->iccIdxLast[i] = 0;
    }

    hPsData->iidEnable = hPsData->iidEnableLast = 0;
    hPsData->iccEnable = hPsData->iccEnableLast = 0;
    hPsData->ipdEnable = hPsData->ipdEnableLast = 0;
    hPsData->ipdTimeCnt = MAX_TIME_DIFF_FRAMES;
    for (i = 0; i < PS_MAX_BANDS; i++) {
      hPsData->ipdIdxLast[i] = 0;
      hPsData->opdIdxLast[i] = 0;
    }
    for (env = 0; env < PS_MAX_ENVELOPES; env++) {
      hPsData->ipdDiffMode[env] = PS_DELTA_FREQ;
      hPsData->opdDiffMode[env] = PS_DELTA_FREQ;
      for (i = 0; i < PS_MAX_BANDS; i++) {
        hPsData->ipdIdx[env][i] = 0;
        hPsData->opdIdx[env][i] = 0;
      }
    }
    hPsData->iidQuantMode = hPsData->iidQuantModeLast = PS_IID_RES_COARSE;
    hPsData->iccQuantMode = hPsData->iccQuantModeLast = PS_ICC_ROT_A;

    for (env = 0; env < PS_MAX_ENVELOPES; env++) {
      hPsData->iccDiffMode[env] = PS_DELTA_FREQ;
      hPsData->iccDiffMode[env] = PS_DELTA_FREQ;

      for (i = 0; i < PS_MAX_BANDS; i++) {
        hPsData->iidIdx[env][i] = 0;
        hPsData->iccIdx[env][i] = 0;
      }
    }

    hPsData->nEnvelopesLast = 0;

    hPsData->headerCnt = MAX_PS_NOHEADER_CNT;
    hPsData->iidTimeCnt = MAX_TIME_DIFF_FRAMES;
    hPsData->iccTimeCnt = MAX_TIME_DIFF_FRAMES;
    hPsData->noEnvCnt = MAX_NOENV_CNT;
  }

  return error;
}

static FIXP_DBL quantizeCoef(const FIXP_DBL *RESTRICT input, const INT nBands,
                             const FIXP_DBL *RESTRICT quantTable,
                             const INT idxOffset, const INT nQuantSteps,
                             INT *RESTRICT quantOut) {
  INT idx, band;
  FIXP_DBL quantErr = FL2FXCONST_DBL(0.f);

  for (band = 0; band < nBands; band++) {
    for (idx = 0; idx < nQuantSteps - 1; idx++) {
      if (fixp_abs((input[band] >> 1) - (quantTable[idx + 1] >> 1)) >
          fixp_abs((input[band] >> 1) - (quantTable[idx] >> 1))) {
        break;
      }
    }
    quantErr += (fixp_abs(input[band] - quantTable[idx]) >>
                 PS_QUANT_SCALE); /* don't scale before subtraction; diff
                                     smaller (64-25)/64 */
    quantOut[band] = idx - idxOffset;
  }

  return quantErr;
}

static INT getICCMode(const INT nBands, const INT rotType) {
  INT mode = 0;

  switch (nBands) {
    case PS_BANDS_COARSE:
      mode = PS_RES_COARSE;
      break;
    case PS_BANDS_MID:
      mode = PS_RES_MID;
      break;
    default:
      mode = 0;
  }
  if (rotType == PS_ICC_ROT_B) {
    mode += 3;
  }

  return mode;
}

static INT getIIDMode(const INT nBands, const INT iidRes) {
  INT mode = 0;

  switch (nBands) {
    case PS_BANDS_COARSE:
      mode = PS_RES_COARSE;
      break;
    case PS_BANDS_MID:
      mode = PS_RES_MID;
      break;
    default:
      mode = 0;
      break;
  }

  if (iidRes == PS_IID_RES_FINE) {
    mode += 3;
  }

  return mode;
}

static INT envelopeReducible(FIXP_DBL iid[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                             FIXP_DBL icc[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                             INT psBands, INT nEnvelopes) {
#define THRESH_SCALE 7

  INT reducible = 1; /* true */
  INT e = 0, b = 0;
  FIXP_DBL dIid = FL2FXCONST_DBL(0.f);
  FIXP_DBL dIcc = FL2FXCONST_DBL(0.f);

  FIXP_DBL iidErrThreshold, iccErrThreshold;
  FIXP_DBL iidMeanError, iccMeanError;

  /* Frankenstein: --ps-env-reduce 0 disables this automatic halving loop.
   * Stock FDK keeps collapsing the envelope count (4 -> 2 -> 1) as long as the
   * mean IID/ICC error between neighbouring envelopes stays under a hardcoded
   * threshold, so the number of envelopes actually transmitted is often LOWER
   * than the configured maximum. Turning the loop off makes --ps-env literal:
   * the requested time resolution is always spent. */
  if (g_franken.psEnvReduce == 0) {
    return 0;
  }

  /* square values to prevent sqrt,
     multiply bands to prevent division; bands shifted DFRACT_BITS instead
     (DFRACT_BITS-1) because fMultDiv2 used*/
  iidErrThreshold =
      fMultDiv2(FL2FXCONST_DBL(6.5f * 6.5f / (IID_SCALE_FT * IID_SCALE_FT)),
                (FIXP_DBL)(psBands << ((DFRACT_BITS)-THRESH_SCALE)));
  iccErrThreshold =
      fMultDiv2(FL2FXCONST_DBL(0.75f * 0.75f),
                (FIXP_DBL)(psBands << ((DFRACT_BITS)-THRESH_SCALE)));

  if (nEnvelopes <= 1) {
    reducible = 0;
  } else {
    /* mean error criterion */
    for (e = 0; (e < nEnvelopes / 2) && (reducible != 0); e++) {
      iidMeanError = iccMeanError = FL2FXCONST_DBL(0.f);
      for (b = 0; b < psBands; b++) {
        dIid = (iid[2 * e][b] >> 1) -
               (iid[2 * e + 1][b] >> 1); /* scale 1 bit; squared -> 2 bit */
        dIcc = (icc[2 * e][b] >> 1) - (icc[2 * e + 1][b] >> 1);
        iidMeanError += fPow2Div2(dIid) >> (5 - 1); /* + (bands=20) scale = 5 */
        iccMeanError += fPow2Div2(dIcc) >> (5 - 1);
      } /* --> scaling = 7 bit = THRESH_SCALE !! */

      /* instead sqrt values are squared!
         instead of division, multiply threshold with psBands
         scaling necessary!! */

      /* quit as soon as threshold is reached */
      if ((iidMeanError > (iidErrThreshold)) ||
          (iccMeanError > (iccErrThreshold))) {
        reducible = 0;
      }
    }
  } /* nEnvelopes != 1 */

  return reducible;
}

static void processIidData(PS_DATA *psData,
                           FIXP_DBL iid[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                           const INT psBands, const INT nEnvelopes,
                           const FIXP_DBL quantErrorThreshold) {
  INT iidIdxFine[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  INT iidIdxCoarse[PS_MAX_ENVELOPES][PS_MAX_BANDS];

  FIXP_DBL errIID = FL2FXCONST_DBL(0.f);
  FIXP_DBL errIIDFine = FL2FXCONST_DBL(0.f);
  INT bitsIidFreq = 0;
  INT bitsIidTime = 0;
  INT bitsFineTot = 0;
  INT bitsCoarseTot = 0;
  INT error = 0;
  INT env, band;
  INT diffMode[PS_MAX_ENVELOPES], diffModeFine[PS_MAX_ENVELOPES];
  INT loudnDiff = 0;
  INT iidTransmit = 0;

  /* Quantize IID coefficients */
  for (env = 0; env < nEnvelopes; env++) {
    errIID +=
        quantizeCoef(iid[env], psBands, iidQuant_fx, 7, 15, iidIdxCoarse[env]);
    errIIDFine += quantizeCoef(iid[env], psBands, iidQuantFine_fx, 15, 31,
                               iidIdxFine[env]);
  }

  /* normalize error to number of envelopes, ps bands
     errIID /= psBands*nEnvelopes;
     errIIDFine /= psBands*nEnvelopes; */

  /* Check if IID coefficients should be used in this frame */
  psData->iidEnable = 0;
  for (env = 0; env < nEnvelopes; env++) {
    for (band = 0; band < psBands; band++) {
      loudnDiff += fixp_abs(iidIdxCoarse[env][band]);
      iidTransmit++;
    }
  }

  if (loudnDiff >
      fMultI(FL2FXCONST_DBL(0.7f), iidTransmit)) { /* 0.7f empiric value */
    psData->iidEnable = 1;
  }

  /* Frankenstein: force PS IID parameter sending on/off, overriding the
   * loudness-difference heuristic. 0 = never send IID (flattens stereo image),
   * 1 = always send. */
  if (g_franken.psEnable == 0) {
    psData->iidEnable = 0;
  } else if (g_franken.psEnable == 1) {
    psData->iidEnable = 1;
  }

  /* if iid not active -> RESET data */
  if (psData->iidEnable == 0) {
    psData->iidTimeCnt = MAX_TIME_DIFF_FRAMES;
    for (env = 0; env < nEnvelopes; env++) {
      psData->iidDiffMode[env] = PS_DELTA_FREQ;
      FDKmemclear(psData->iidIdx[env], sizeof(INT) * psBands);
    }
    return;
  }

  /* count COARSE quantization bits for first envelope*/
  bitsIidFreq = FDKsbrEnc_EncodeIid(NULL, iidIdxCoarse[0], NULL, psBands,
                                    PS_IID_RES_COARSE, PS_DELTA_FREQ, &error);

  if ((psData->iidTimeCnt >= MAX_TIME_DIFF_FRAMES) ||
      (psData->iidQuantModeLast == PS_IID_RES_FINE)) {
    bitsIidTime = DO_NOT_USE_THIS_MODE;
  } else {
    bitsIidTime =
        FDKsbrEnc_EncodeIid(NULL, iidIdxCoarse[0], psData->iidIdxLast, psBands,
                            PS_IID_RES_COARSE, PS_DELTA_TIME, &error);
  }

  /* decision DELTA_FREQ vs DELTA_TIME */
  if (bitsIidTime > bitsIidFreq) {
    diffMode[0] = PS_DELTA_FREQ;
    bitsCoarseTot = bitsIidFreq;
  } else {
    diffMode[0] = PS_DELTA_TIME;
    bitsCoarseTot = bitsIidTime;
  }

  /* count COARSE quantization bits for following envelopes*/
  for (env = 1; env < nEnvelopes; env++) {
    bitsIidFreq = FDKsbrEnc_EncodeIid(NULL, iidIdxCoarse[env], NULL, psBands,
                                      PS_IID_RES_COARSE, PS_DELTA_FREQ, &error);
    bitsIidTime =
        FDKsbrEnc_EncodeIid(NULL, iidIdxCoarse[env], iidIdxCoarse[env - 1],
                            psBands, PS_IID_RES_COARSE, PS_DELTA_TIME, &error);

    /* decision DELTA_FREQ vs DELTA_TIME */
    if (bitsIidTime > bitsIidFreq) {
      diffMode[env] = PS_DELTA_FREQ;
      bitsCoarseTot += bitsIidFreq;
    } else {
      diffMode[env] = PS_DELTA_TIME;
      bitsCoarseTot += bitsIidTime;
    }
  }

  /* count FINE quantization bits for first envelope*/
  bitsIidFreq = FDKsbrEnc_EncodeIid(NULL, iidIdxFine[0], NULL, psBands,
                                    PS_IID_RES_FINE, PS_DELTA_FREQ, &error);

  if ((psData->iidTimeCnt >= MAX_TIME_DIFF_FRAMES) ||
      (psData->iidQuantModeLast == PS_IID_RES_COARSE)) {
    bitsIidTime = DO_NOT_USE_THIS_MODE;
  } else {
    bitsIidTime =
        FDKsbrEnc_EncodeIid(NULL, iidIdxFine[0], psData->iidIdxLast, psBands,
                            PS_IID_RES_FINE, PS_DELTA_TIME, &error);
  }

  /* decision DELTA_FREQ vs DELTA_TIME */
  if (bitsIidTime > bitsIidFreq) {
    diffModeFine[0] = PS_DELTA_FREQ;
    bitsFineTot = bitsIidFreq;
  } else {
    diffModeFine[0] = PS_DELTA_TIME;
    bitsFineTot = bitsIidTime;
  }

  /* count FINE quantization bits for following envelopes*/
  for (env = 1; env < nEnvelopes; env++) {
    bitsIidFreq = FDKsbrEnc_EncodeIid(NULL, iidIdxFine[env], NULL, psBands,
                                      PS_IID_RES_FINE, PS_DELTA_FREQ, &error);
    bitsIidTime =
        FDKsbrEnc_EncodeIid(NULL, iidIdxFine[env], iidIdxFine[env - 1], psBands,
                            PS_IID_RES_FINE, PS_DELTA_TIME, &error);

    /* decision DELTA_FREQ vs DELTA_TIME */
    if (bitsIidTime > bitsIidFreq) {
      diffModeFine[env] = PS_DELTA_FREQ;
      bitsFineTot += bitsIidFreq;
    } else {
      diffModeFine[env] = PS_DELTA_TIME;
      bitsFineTot += bitsIidTime;
    }
  }

  if (bitsFineTot == bitsCoarseTot) {
    /* if same number of bits is needed, use the quantization with lower error
     */
    if (errIIDFine < errIID) {
      bitsCoarseTot = DO_NOT_USE_THIS_MODE;
    } else {
      bitsFineTot = DO_NOT_USE_THIS_MODE;
    }
  } else {
    /* const FIXP_DBL minThreshold =
     * FL2FXCONST_DBL(0.2f/(IID_SCALE_FT*PS_QUANT_SCALE_FT)*(psBands*nEnvelopes));
     */
    const FIXP_DBL minThreshold =
        (FIXP_DBL)((LONG)0x00019999 * (psBands * nEnvelopes));

    /* decision RES_FINE vs RES_COARSE                 */
    /* test if errIIDFine*quantErrorThreshold < errIID */
    /* shiftVal 2 comes from scaling of quantErrorThreshold */
    if (fixMax(((errIIDFine >> 1) + (minThreshold >> 1)) >> 1,
               fMult(quantErrorThreshold, errIIDFine)) < (errIID >> 2)) {
      bitsCoarseTot = DO_NOT_USE_THIS_MODE;
    } else if (fixMax(((errIID >> 1) + (minThreshold >> 1)) >> 1,
                      fMult(quantErrorThreshold, errIID)) < (errIIDFine >> 2)) {
      bitsFineTot = DO_NOT_USE_THIS_MODE;
    }
  }

  /* decision RES_FINE vs RES_COARSE */
  if (bitsFineTot < bitsCoarseTot) {
    psData->iidQuantMode = PS_IID_RES_FINE;
    for (env = 0; env < nEnvelopes; env++) {
      psData->iidDiffMode[env] = diffModeFine[env];
      FDKmemcpy(psData->iidIdx[env], iidIdxFine[env], psBands * sizeof(INT));
    }
  } else {
    psData->iidQuantMode = PS_IID_RES_COARSE;
    for (env = 0; env < nEnvelopes; env++) {
      psData->iidDiffMode[env] = diffMode[env];
      FDKmemcpy(psData->iidIdx[env], iidIdxCoarse[env], psBands * sizeof(INT));
    }
  }

  /* Frankenstein: force PS IID quantisation mode regardless of the bit/error
   * decision above. 0 = coarse, 1 = fine. Copies the matching index set so the
   * chosen resolution is actually what gets written. */
  if (g_franken.psIidQuant == 0 && psData->iidQuantMode != PS_IID_RES_COARSE) {
    psData->iidQuantMode = PS_IID_RES_COARSE;
    for (env = 0; env < nEnvelopes; env++) {
      psData->iidDiffMode[env] = diffMode[env];
      FDKmemcpy(psData->iidIdx[env], iidIdxCoarse[env], psBands * sizeof(INT));
    }
  } else if (g_franken.psIidQuant == 1 &&
             psData->iidQuantMode != PS_IID_RES_FINE) {
    psData->iidQuantMode = PS_IID_RES_FINE;
    for (env = 0; env < nEnvelopes; env++) {
      psData->iidDiffMode[env] = diffModeFine[env];
      FDKmemcpy(psData->iidIdx[env], iidIdxFine[env], psBands * sizeof(INT));
    }
  }

  /* Count DELTA_TIME encoding streaks */
  for (env = 0; env < nEnvelopes; env++) {
    if (psData->iidDiffMode[env] == PS_DELTA_TIME)
      psData->iidTimeCnt++;
    else
      psData->iidTimeCnt = 0;
  }
}

static INT similarIid(PS_DATA *psData, const INT psBands,
                      const INT nEnvelopes) {
  const INT diffThr = (psData->iidQuantMode == PS_IID_RES_COARSE) ? 2 : 3;
  const INT sumDiffThr = diffThr * psBands / 4;
  INT similar = 0;
  INT diff = 0;
  INT sumDiff = 0;
  INT env = 0;
  INT b = 0;
  if ((nEnvelopes == psData->nEnvelopesLast) && (nEnvelopes == 1)) {
    similar = 1;
    for (env = 0; env < nEnvelopes; env++) {
      sumDiff = 0;
      b = 0;
      do {
        diff = fixp_abs(psData->iidIdx[env][b] - psData->iidIdxLast[b]);
        sumDiff += diff;
        if ((diff > diffThr) /* more than x quantization steps in any band */
            || (sumDiff > sumDiffThr)) { /* more than x quantisations steps
                                            overall difference */
          similar = 0;
        }
        b++;
      } while ((b < psBands) && (similar > 0));
    }
  } /* nEnvelopes==1  */

  return similar;
}

static INT similarIcc(PS_DATA *psData, const INT psBands,
                      const INT nEnvelopes) {
  const INT diffThr = 2;
  const INT sumDiffThr = diffThr * psBands / 4;
  INT similar = 0;
  INT diff = 0;
  INT sumDiff = 0;
  INT env = 0;
  INT b = 0;
  if ((nEnvelopes == psData->nEnvelopesLast) && (nEnvelopes == 1)) {
    similar = 1;
    for (env = 0; env < nEnvelopes; env++) {
      sumDiff = 0;
      b = 0;
      do {
        diff = fixp_abs(psData->iccIdx[env][b] - psData->iccIdxLast[b]);
        sumDiff += diff;
        if ((diff > diffThr) /* more than x quantisation step in any band */
            || (sumDiff > sumDiffThr)) { /* more than x quantisations steps
                                            overall difference */
          similar = 0;
        }
        b++;
      } while ((b < psBands) && (similar > 0));
    }
  } /* nEnvelopes==1  */

  return similar;
}

static void processIccData(
    PS_DATA *psData,
    FIXP_DBL icc[PS_MAX_ENVELOPES][PS_MAX_BANDS], /* const input values:
                                                     unable to declare as
                                                     const, since it does
                                                     not poINT to const
                                                     memory */
    const INT psBands, const INT nEnvelopes) {
  FIXP_DBL errICC = FL2FXCONST_DBL(0.f);
  INT env, band;
  INT bitsIccFreq, bitsIccTime;
  INT error = 0;
  INT inCoherence = 0, iccTransmit = 0;
  INT *iccIdxLast;

  iccIdxLast = psData->iccIdxLast;

  /* Quantize ICC coefficients */
  for (env = 0; env < nEnvelopes; env++) {
    errICC +=
        quantizeCoef(icc[env], psBands, iccQuant, 0, 8, psData->iccIdx[env]);
  }

  /* Check if ICC coefficients should be used */
  psData->iccEnable = 0;
  for (env = 0; env < nEnvelopes; env++) {
    for (band = 0; band < psBands; band++) {
      inCoherence += psData->iccIdx[env][band];
      iccTransmit++;
    }
  }
  if (inCoherence >
      fMultI(FL2FXCONST_DBL(0.5f), iccTransmit)) { /* 0.5f empiric value */
    psData->iccEnable = 1;
  }

  /* Frankenstein: force ICC (interchannel coherence) on/off and rotation mode. */
  if (g_franken.psIcc == 0) psData->iccEnable = 0;
  else if (g_franken.psIcc == 1) psData->iccEnable = 1;
  if (g_franken.psIccMode == 0) psData->iccQuantMode = PS_ICC_ROT_A;
  else if (g_franken.psIccMode == 1) psData->iccQuantMode = PS_ICC_ROT_B;

  if (psData->iccEnable == 0) {
    psData->iccTimeCnt = MAX_TIME_DIFF_FRAMES;
    for (env = 0; env < nEnvelopes; env++) {
      psData->iccDiffMode[env] = PS_DELTA_FREQ;
      FDKmemclear(psData->iccIdx[env], sizeof(INT) * psBands);
    }
    return;
  }

  for (env = 0; env < nEnvelopes; env++) {
    bitsIccFreq = FDKsbrEnc_EncodeIcc(NULL, psData->iccIdx[env], NULL, psBands,
                                      PS_DELTA_FREQ, &error);

    if (psData->iccTimeCnt < MAX_TIME_DIFF_FRAMES) {
      bitsIccTime = FDKsbrEnc_EncodeIcc(NULL, psData->iccIdx[env], iccIdxLast,
                                        psBands, PS_DELTA_TIME, &error);
    } else {
      bitsIccTime = DO_NOT_USE_THIS_MODE;
    }

    if (bitsIccFreq > bitsIccTime) {
      psData->iccDiffMode[env] = PS_DELTA_TIME;
      psData->iccTimeCnt++;
    } else {
      psData->iccDiffMode[env] = PS_DELTA_FREQ;
      psData->iccTimeCnt = 0;
    }
    iccIdxLast = psData->iccIdx[env];
  }
}

static void calculateIID(FIXP_DBL ldPwrL[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL ldPwrR[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL iid[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         INT nEnvelopes, INT psBands) {
  INT i = 0;
  INT env = 0;
  for (env = 0; env < nEnvelopes; env++) {
    for (i = 0; i < psBands; i++) {
      /* iid[env][i] = 10.0f*(float)log10(pwrL[env][i]/pwrR[env][i]);
       */
      FIXP_DBL IID = fMultDiv2(FL2FXCONST_DBL(LOG10_2_10 / IID_SCALE_FT),
                               (ldPwrL[env][i] - ldPwrR[env][i]));

      IID = fixMin(IID, (FIXP_DBL)(MAXVAL_DBL >> (LD_DATA_SHIFT + 1)));
      IID = fixMax(IID, (FIXP_DBL)(MINVAL_DBL >> (LD_DATA_SHIFT + 1)));
      iid[env][i] = IID << (LD_DATA_SHIFT + 1);
    }
  }
}

/* Number of IPD parameter bands for a given PS band count. MPEG-4 PS codes the
 * inter-channel PHASE only for the lower part of the spectrum: 5 bands when 10
 * stereo bands are used, 11 when 20 are used (17 for the 34-band mode, which
 * this encoder never produces). Verified against ffmpeg's independent decoder
 * (libavcodec/aacps_common.c: nr_iidopd_par_tab[] = {5, 11, 17, 5, 11, 17}),
 * and it coincides exactly with the border where calculateICC() below switches
 * from the signed real-part coherence to the magnitude form. */
static INT getIpdBands(const INT psBands) {
  switch (psBands) {
    case PS_BANDS_COARSE:
      return 5;
    case PS_BANDS_MID:
      return 11;
    default:
      return 5;
  }
}

/* Frankenstein: derive the Inter-channel Phase Difference from the complex
 * cross-spectrum the encoder already accumulates.
 *
 * pwrCr / pwrCi are the real and imaginary parts of sum(L * conj(R)) per band
 * and envelope. calculateICC() uses only their MAGNITUDE and throws the angle
 * away; stock FDK then hardcodes IPD to zero ("IPD OPD not supported right
 * now"). The angle is exactly what IPD is, so no new signal analysis is needed
 * here - just atan2 over data that is already sitting in the same loop.
 *
 * Quantisation: 8 steps of pi/4 covering the full circle, matching the 3-bit
 * parameter the decoder expects (ffmpeg masks the accumulated value with &0x07
 * and looks the angle up in an 8-entry sin/cos table). fixp_atan2 returns Q29
 * over ]-pi .. pi], so index = round(phi / (pi/4)) taken modulo 8. */
/* Frankenstein: OPD - the phase of the LEFT channel relative to the MONO downmix.
 *
 * The decoder applies the two phase parameters like this (ffmpeg aacps.c and
 * aacpsdsp_template.c ps_stereo_interpolate_ipdopd_c):
 *     left  = e^{i*OPD}         * (h11*mono + h21*decorrelated)
 *     right = e^{i*(OPD - IPD)} * (h12*mono + h22*decorrelated)
 * so the DIFFERENCE arg(left) - arg(right) always comes out as IPD, but where
 * that rotation sits relative to the downmix is set by OPD. With OPD = 0 the
 * left channel is pinned to the downmix phase and the entire rotation is dumped
 * onto the right one - the difference is right, its placement is not.
 *
 * Computing it needs NO new data. The downmix is L + R, so
 *     sum(L * conj(L+R)) = sum(|L|^2) + sum(L * conj(R))
 * whose real part is (pwrL + pwrCr) and whose imaginary part is exactly pwrCi -
 * all three accumulators already exist in this loop. Hence
 *     OPD = atan2(pwrCi, pwrL + pwrCr).
 * (For the equal-level delay case this reduces analytically to IPD/2, which is a
 * useful sanity check, but the general form above also handles level-imbalanced
 * bands correctly, where IPD/2 would be wrong.) */
static void calculateOpd(FIXP_DBL pwrL[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL pwrCr[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL pwrCi[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         INT opd[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         const INT nEnvelopes, const INT psBands) {
  const INT opdBands = fixMin(getIpdBands(psBands), psBands);
  const LONG qStepQ29 = (LONG)((1u << Q_ATAN2OUT) * 0.78539816339744831f);
  const LONG qHalfQ29 = qStepQ29 >> 1;
  INT env, band;

  for (env = 0; env < nEnvelopes; env++) {
    for (band = 0; band < PS_MAX_BANDS; band++) {
      opd[env][band] = 0;
    }
    for (band = 0; band < opdBands; band++) {
      /* Re{L * conj(L+R)} = |L|^2 + Re{L * conj(R)}. Both terms are already
       * scaled the same way (they come from the same accumulation loop), so
       * they can be added directly; >>1 on each keeps the sum from overflowing. */
      FIXP_DBL re = (pwrL[env][band] >> 1) + (pwrCr[env][band] >> 1);
      FIXP_DBL im = pwrCi[env][band] >> 1;
      LONG phi, idx;

      if ((re == (FIXP_DBL)0) && (im == (FIXP_DBL)0)) {
        continue;
      }
      phi = (LONG)fixp_atan2(im, re);
      idx = (phi >= 0) ? ((phi + qHalfQ29) / qStepQ29)
                       : -((-phi + qHalfQ29) / qStepQ29);
      idx &= 0x07;
      opd[env][band] = (INT)idx;
    }
  }
}

static void calculateIpd(FIXP_DBL pwrCr[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL pwrCi[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         INT ipd[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         const INT nEnvelopes, const INT psBands) {
  const INT ipdBands = fixMin(getIpdBands(psBands), psBands);
  INT env, band;

  /* pi/4 in the Q29 output scale of fixp_atan2, and half of it for rounding. */
  const LONG qStepQ29 = (LONG)((1u << Q_ATAN2OUT) * 0.78539816339744831f);
  const LONG qHalfQ29 = qStepQ29 >> 1;

  for (env = 0; env < nEnvelopes; env++) {
    for (band = 0; band < PS_MAX_BANDS; band++) {
      ipd[env][band] = 0;
    }
    for (band = 0; band < ipdBands; band++) {
      LONG phi, idx;

      /* Both parts zero means no meaningful phase (silent band) -> index 0. */
      if ((pwrCr[env][band] == (FIXP_DBL)0) &&
          (pwrCi[env][band] == (FIXP_DBL)0)) {
        continue;
      }

      /* The accumulator IS already the L-relative-to-R cross-spectrum:
       *   pwrCr = sum(Re(L)Re(R) + Im(L)Im(R))
       *   pwrCi = sum(Re(R)Im(L) - Re(L)Im(R))
       * which is exactly Re and Im of sum(L * conj(R)), so
       *   atan2(pwrCi, pwrCr) = arg(L) - arg(R)
       * i.e. the phase of the left channel relative to the right - the
       * definition of IPD. No sign flip is needed here (an earlier attempt
       * negated pwrCi and measurably made the phase error worse). */
      phi = (LONG)fixp_atan2(pwrCi[env][band], pwrCr[env][band]);

      /* Round to the nearest multiple of pi/4, then wrap into 0..7. */
      idx = (phi >= 0) ? ((phi + qHalfQ29) / qStepQ29)
                       : -((-phi + qHalfQ29) / qStepQ29);
      idx &= 0x07;
      ipd[env][band] = (INT)idx;
    }
  }
}

/* Decide delta-frequency vs delta-time coding for IPD, mirroring what
 * processIidData/processIccData do for the other parameters: count the bits
 * each mode would need and keep the cheaper one. */
static void processIpdData(PS_DATA *psData,
                           INT ipd[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                           INT opd[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                           const INT psBands, const INT nEnvelopes) {
  const INT ipdBands = fixMin(getIpdBands(psBands), psBands);
  INT env, band, error = 0;

  for (env = 0; env < nEnvelopes; env++) {
    INT bitsFreq, bitsTime;
    /* Delta-time predicts from the previous envelope of THIS frame for every
     * envelope after the first, and only envelope 0 looks back at the previous
     * frame - this must match the writer (ps_bitenc.cpp) and the decoder
     * (ffmpeg aacps_common.c: e_prev = e ? e - 1 : num_env_old - 1), otherwise
     * the mode is chosen against a base that nobody else uses. */
    const INT *ipdRef = (env > 0) ? psData->ipdIdx[env - 1] : psData->ipdIdxLast;
    const INT *opdRef = (env > 0) ? psData->opdIdx[env - 1] : psData->opdIdxLast;
    /* Envelope 0 is the only one that needs a valid PREVIOUS FRAME; later
     * envelopes reference this frame and are therefore always predictable. */
    const INT timeUsable =
        (env > 0) || (psData->ipdTimeCnt < MAX_TIME_DIFF_FRAMES);

    for (band = 0; band < PS_MAX_BANDS; band++) {
      psData->ipdIdx[env][band] = ipd[env][band];
      psData->opdIdx[env][band] = opd[env][band];
    }

    /* OPD uses the same delta-mode choice as IPD: the two are written back to
     * back per envelope and follow the same statistics. */
    {
      INT of = FDKsbrEnc_EncodeOpd(NULL, psData->opdIdx[env], NULL, ipdBands,
                                   PS_DELTA_FREQ, &error);
      INT ot = (!timeUsable)
                   ? DO_NOT_USE_THIS_MODE
                   : FDKsbrEnc_EncodeOpd(NULL, psData->opdIdx[env], opdRef,
                                         ipdBands, PS_DELTA_TIME, &error);
      psData->opdDiffMode[env] = (ot < of) ? PS_DELTA_TIME : PS_DELTA_FREQ;
    }

    bitsFreq = FDKsbrEnc_EncodeIpd(NULL, psData->ipdIdx[env], NULL, ipdBands,
                                   PS_DELTA_FREQ, &error);
    /* Delta-time needs a valid previous frame; on the first frame (or right
     * after a reset) fall back to delta-frequency. */
    if (!timeUsable) {
      bitsTime = DO_NOT_USE_THIS_MODE;
    } else {
      bitsTime = FDKsbrEnc_EncodeIpd(NULL, psData->ipdIdx[env], ipdRef,
                                     ipdBands, PS_DELTA_TIME, &error);
    }

    psData->ipdDiffMode[env] =
        (bitsTime < bitsFreq) ? PS_DELTA_TIME : PS_DELTA_FREQ;
  }

  /* NOTE: ipdIdxLast is deliberately NOT updated here. It must still hold the
   * PREVIOUS frame's values while the bitstream for this frame is written,
   * because that is what delta-time coding is relative to (and what the
   * decoder will have). It is rolled forward at the end of FDKsbrEnc_PSEncode,
   * together with iidIdxLast/iccIdxLast - same ordering as IID/ICC. */
}

static void calculateICC(FIXP_DBL pwrL[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL pwrR[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL pwrCr[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL pwrCi[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         FIXP_DBL icc[PS_MAX_ENVELOPES][PS_MAX_BANDS],
                         INT nEnvelopes, INT psBands) {
  INT i = 0;
  INT env = 0;
  INT border = psBands;

  switch (psBands) {
    case PS_BANDS_COARSE:
      border = 5;
      break;
    case PS_BANDS_MID:
      border = 11;
      break;
    default:
      break;
  }

  for (env = 0; env < nEnvelopes; env++) {
    for (i = 0; i < border; i++) {
      /* icc[env][i] = min( pwrCr[env][i] / (float) sqrt(pwrL[env][i] *
       * pwrR[env][i]) , 1.f);
       */
      int scale;
      FIXP_DBL invNrg = invSqrtNorm2(
          fMax(fMult(pwrL[env][i], pwrR[env][i]), (FIXP_DBL)1), &scale);
      icc[env][i] =
          SATURATE_LEFT_SHIFT(fMult(pwrCr[env][i], invNrg), scale, DFRACT_BITS);
    }

    for (; i < psBands; i++) {
      int denom_e;
      FIXP_DBL denom_m = fMultNorm(pwrL[env][i], pwrR[env][i], &denom_e);

      if (denom_m == (FIXP_DBL)0) {
        icc[env][i] = (FIXP_DBL)MAXVAL_DBL;
      } else {
        int num_e, result_e;
        FIXP_DBL num_m, result_m;

        num_e = CountLeadingBits(
            fixMax(fixp_abs(pwrCr[env][i]), fixp_abs(pwrCi[env][i])));
        num_m = fPow2Div2((pwrCr[env][i] << num_e)) +
                fPow2Div2((pwrCi[env][i] << num_e));

        result_m = fDivNorm(num_m, denom_m, &result_e);
        result_e += (-2 * num_e + 1) - denom_e;
        icc[env][i] = scaleValueSaturate(sqrtFixp(result_m >> (result_e & 1)),
                                         (result_e + (result_e & 1)) >> 1);
      }
    }
  }
}

void FDKsbrEnc_initPsBandNrgScale(HANDLE_PS_ENCODE hPsEncode) {
  INT group, bin;
  INT nIidGroups = hPsEncode->nQmfIidGroups + hPsEncode->nSubQmfIidGroups;

  FDKmemclear(hPsEncode->psBandNrgScale, PS_MAX_BANDS * sizeof(SCHAR));

  for (group = 0; group < nIidGroups; group++) {
    /* Translate group to bin */
    bin = hPsEncode->subband2parameterIndex[group];

    /* Translate from 20 bins to 10 bins */
    if (hPsEncode->psEncMode == PS_BANDS_COARSE) {
      bin = bin >> 1;
    }

    hPsEncode->psBandNrgScale[bin] =
        (hPsEncode->psBandNrgScale[bin] == 0)
            ? (hPsEncode->iidGroupWidthLd[group] + 5)
            : (fixMax(hPsEncode->iidGroupWidthLd[group],
                      hPsEncode->psBandNrgScale[bin]) +
               1);
  }
}

FDK_PSENC_ERROR FDKsbrEnc_CreatePSEncode(HANDLE_PS_ENCODE *phPsEncode) {
  FDK_PSENC_ERROR error = PSENC_OK;

  if (phPsEncode == NULL) {
    error = PSENC_INVALID_HANDLE;
  } else {
    HANDLE_PS_ENCODE hPsEncode = NULL;
    if (NULL == (hPsEncode = GetRam_PsEncode())) {
      error = PSENC_MEMORY_ERROR;
      goto bail;
    }
    FDKmemclear(hPsEncode, sizeof(PS_ENCODE));
    *phPsEncode = hPsEncode; /* return allocated handle */
  }
bail:
  return error;
}

FDK_PSENC_ERROR FDKsbrEnc_InitPSEncode(HANDLE_PS_ENCODE hPsEncode,
                                       const PS_BANDS psEncMode,
                                       const FIXP_DBL iidQuantErrorThreshold) {
  FDK_PSENC_ERROR error = PSENC_OK;

  if (NULL == hPsEncode) {
    error = PSENC_INVALID_HANDLE;
  } else {
    if (PSENC_OK != (InitPSData(&hPsEncode->psData))) {
      goto bail;
    }

    switch (psEncMode) {
      case PS_BANDS_COARSE:
      case PS_BANDS_MID:
        hPsEncode->nQmfIidGroups = QMF_GROUPS_LO_RES;
        hPsEncode->nSubQmfIidGroups = SUBQMF_GROUPS_LO_RES;
        FDKmemcpy(hPsEncode->iidGroupBorders, iidGroupBordersLoRes,
                  (hPsEncode->nQmfIidGroups + hPsEncode->nSubQmfIidGroups + 1) *
                      sizeof(INT));
        FDKmemcpy(hPsEncode->subband2parameterIndex, subband2parameter20,
                  (hPsEncode->nQmfIidGroups + hPsEncode->nSubQmfIidGroups) *
                      sizeof(INT));
        FDKmemcpy(hPsEncode->iidGroupWidthLd, iidGroupWidthLdLoRes,
                  (hPsEncode->nQmfIidGroups + hPsEncode->nSubQmfIidGroups) *
                      sizeof(UCHAR));
        break;
      default:
        error = PSENC_INIT_ERROR;
        goto bail;
    }

    hPsEncode->psEncMode = psEncMode;
    hPsEncode->iidQuantErrorThreshold = iidQuantErrorThreshold;
    FDKsbrEnc_initPsBandNrgScale(hPsEncode);
  }
bail:
  return error;
}

FDK_PSENC_ERROR FDKsbrEnc_DestroyPSEncode(HANDLE_PS_ENCODE *phPsEncode) {
  FDK_PSENC_ERROR error = PSENC_OK;

  if (NULL != phPsEncode) {
    FreeRam_PsEncode(phPsEncode);
  }

  return error;
}

typedef struct {
  FIXP_DBL pwrL[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  FIXP_DBL pwrR[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  FIXP_DBL ldPwrL[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  FIXP_DBL ldPwrR[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  FIXP_DBL pwrCr[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  FIXP_DBL pwrCi[PS_MAX_ENVELOPES][PS_MAX_BANDS];

} PS_PWR_DATA;

FDK_PSENC_ERROR FDKsbrEnc_PSEncode(
    HANDLE_PS_ENCODE hPsEncode, HANDLE_PS_OUT hPsOut, UCHAR *dynBandScale,
    UINT maxEnvelopes,
    FIXP_DBL *hybridData[HYBRID_FRAMESIZE][MAX_PS_CHANNELS][2],
    const INT frameSize, const INT sendHeader) {
  FDK_PSENC_ERROR error = PSENC_OK;

  HANDLE_PS_DATA hPsData = &hPsEncode->psData;
  FIXP_DBL iid[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  FIXP_DBL icc[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  INT ipdQ[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  INT opdQ[PS_MAX_ENVELOPES][PS_MAX_BANDS];
  int envBorder[PS_MAX_ENVELOPES + 1];

  int group, bin, col, subband, band;
  int i = 0;

  int env = 0;
  int psBands = (int)hPsEncode->psEncMode;
  int nIidGroups = hPsEncode->nQmfIidGroups + hPsEncode->nSubQmfIidGroups;
  int nEnvelopes = fixMin(maxEnvelopes, (UINT)PS_MAX_ENVELOPES);

  C_ALLOC_SCRATCH_START(pwrData, PS_PWR_DATA, 1)

  for (env = 0; env < nEnvelopes + 1; env++) {
    envBorder[env] = fMultI(GetInvInt(nEnvelopes), frameSize * env);
  }

  for (env = 0; env < nEnvelopes; env++) {
    /* clear energy array */
    for (band = 0; band < psBands; band++) {
      pwrData->pwrL[env][band] = pwrData->pwrR[env][band] =
          pwrData->pwrCr[env][band] = pwrData->pwrCi[env][band] = FIXP_DBL(1);
    }

    /**** calculate energies and correlation ****/

    /* start with hybrid data */
    for (group = 0; group < nIidGroups; group++) {
      /* Translate group to bin */
      bin = hPsEncode->subband2parameterIndex[group];

      /* Translate from 20 bins to 10 bins */
      if (hPsEncode->psEncMode == PS_BANDS_COARSE) {
        bin >>= 1;
      }

      /* determine group border */
      int bScale = hPsEncode->psBandNrgScale[bin];

      FIXP_DBL pwrL_env_bin = pwrData->pwrL[env][bin];
      FIXP_DBL pwrR_env_bin = pwrData->pwrR[env][bin];
      FIXP_DBL pwrCr_env_bin = pwrData->pwrCr[env][bin];
      FIXP_DBL pwrCi_env_bin = pwrData->pwrCi[env][bin];

      int scale = (int)dynBandScale[bin];
      for (col = envBorder[env]; col < envBorder[env + 1]; col++) {
        for (subband = hPsEncode->iidGroupBorders[group];
             subband < hPsEncode->iidGroupBorders[group + 1]; subband++) {
          FIXP_DBL l_real = (hybridData[col][0][0][subband]) << scale;
          FIXP_DBL l_imag = (hybridData[col][0][1][subband]) << scale;
          FIXP_DBL r_real = (hybridData[col][1][0][subband]) << scale;
          FIXP_DBL r_imag = (hybridData[col][1][1][subband]) << scale;

          pwrL_env_bin += (fPow2Div2(l_real) + fPow2Div2(l_imag)) >> bScale;
          pwrR_env_bin += (fPow2Div2(r_real) + fPow2Div2(r_imag)) >> bScale;
          pwrCr_env_bin +=
              (fMultDiv2(l_real, r_real) + fMultDiv2(l_imag, r_imag)) >> bScale;
          pwrCi_env_bin +=
              (fMultDiv2(r_real, l_imag) - fMultDiv2(l_real, r_imag)) >> bScale;
        }
      }
      /* assure, nrg's of left and right channel are not negative; necessary on
       * 16 bit multiply units */
      pwrData->pwrL[env][bin] = fixMax((FIXP_DBL)0, pwrL_env_bin);
      pwrData->pwrR[env][bin] = fixMax((FIXP_DBL)0, pwrR_env_bin);

      pwrData->pwrCr[env][bin] = pwrCr_env_bin;
      pwrData->pwrCi[env][bin] = pwrCi_env_bin;

    } /* nIidGroups */

    /* calc logarithmic energy */
    LdDataVector(pwrData->pwrL[env], pwrData->ldPwrL[env], psBands);
    LdDataVector(pwrData->pwrR[env], pwrData->ldPwrR[env], psBands);

  } /* nEnvelopes */

  /* calculate iid and icc */
  calculateIID(pwrData->ldPwrL, pwrData->ldPwrR, iid, nEnvelopes, psBands);
  calculateICC(pwrData->pwrL, pwrData->pwrR, pwrData->pwrCr, pwrData->pwrCi,
               icc, nEnvelopes, psBands);

  /*** Envelope Reduction ***/
  while (envelopeReducible(iid, icc, psBands, nEnvelopes)) {
    int e = 0;
    /* sum energies of two neighboring envelopes */
    nEnvelopes >>= 1;
    for (e = 0; e < nEnvelopes; e++) {
      FDKsbrEnc_addFIXP_DBL(pwrData->pwrL[2 * e], pwrData->pwrL[2 * e + 1],
                            pwrData->pwrL[e], psBands);
      FDKsbrEnc_addFIXP_DBL(pwrData->pwrR[2 * e], pwrData->pwrR[2 * e + 1],
                            pwrData->pwrR[e], psBands);
      FDKsbrEnc_addFIXP_DBL(pwrData->pwrCr[2 * e], pwrData->pwrCr[2 * e + 1],
                            pwrData->pwrCr[e], psBands);
      FDKsbrEnc_addFIXP_DBL(pwrData->pwrCi[2 * e], pwrData->pwrCi[2 * e + 1],
                            pwrData->pwrCi[e], psBands);

      /* calc logarithmic energy */
      LdDataVector(pwrData->pwrL[e], pwrData->ldPwrL[e], psBands);
      LdDataVector(pwrData->pwrR[e], pwrData->ldPwrR[e], psBands);

      /* reduce number of envelopes and adjust borders */
      envBorder[e] = envBorder[2 * e];
    }
    envBorder[nEnvelopes] = envBorder[2 * nEnvelopes];

    /* re-calculate iid and icc */
    calculateIID(pwrData->ldPwrL, pwrData->ldPwrR, iid, nEnvelopes, psBands);
    calculateICC(pwrData->pwrL, pwrData->pwrR, pwrData->pwrCr, pwrData->pwrCi,
                 icc, nEnvelopes, psBands);
  }

  /*  */
  if (sendHeader) {
    hPsData->headerCnt = MAX_PS_NOHEADER_CNT;
    hPsData->iidTimeCnt = MAX_TIME_DIFF_FRAMES;
    hPsData->iccTimeCnt = MAX_TIME_DIFF_FRAMES;
    hPsData->noEnvCnt = MAX_NOENV_CNT;
  }

  /*** Parameter processing, quantisation etc ***/
  processIidData(hPsData, iid, psBands, nEnvelopes,
                 hPsEncode->iidQuantErrorThreshold);
  processIccData(hPsData, icc, psBands, nEnvelopes);

  /* Frankenstein: IPD is derived from the FINAL pwrCr/pwrCi, i.e. after any
   * envelope reduction above, so the phase matches the envelopes actually
   * transmitted. Gated on --ps-ipd; when off, nothing is computed and the
   * bitstream is byte-identical to stock. */
  hPsData->ipdEnable = (g_franken.psIpd == 1) ? 1 : 0;
  if (hPsData->ipdEnable) {
    calculateIpd(pwrData->pwrCr, pwrData->pwrCi, ipdQ, nEnvelopes, psBands);
    if (g_franken.psOpd == 0) {
      /* --ps-opd 0: keep OPD at the neutral zero for A/B comparison. */
      INT e2, b2;
      for (e2 = 0; e2 < PS_MAX_ENVELOPES; e2++)
        for (b2 = 0; b2 < PS_MAX_BANDS; b2++) opdQ[e2][b2] = 0;
    } else {
      calculateOpd(pwrData->pwrL, pwrData->pwrCr, pwrData->pwrCi, opdQ,
                   nEnvelopes, psBands);
    }
    processIpdData(hPsData, ipdQ, opdQ, psBands, nEnvelopes);
  } else {
    hPsData->ipdTimeCnt = MAX_TIME_DIFF_FRAMES;
  }

  /*** Initialize output struct ***/

  /* PS Header on/off ? */
  if ((hPsData->headerCnt < MAX_PS_NOHEADER_CNT) &&
      ((hPsData->iidQuantMode == hPsData->iidQuantModeLast) &&
       (hPsData->iccQuantMode == hPsData->iccQuantModeLast)) &&
      ((hPsData->iidEnable == hPsData->iidEnableLast) &&
       (hPsData->iccEnable == hPsData->iccEnableLast)) &&
      /* A change in IPD presence flips the ps_extension flag, so the header
       * must be re-sent for the decoder to pick it up. */
      (hPsData->ipdEnable == hPsData->ipdEnableLast)) {
    hPsOut->enablePSHeader = 0;
  } else {
    hPsOut->enablePSHeader = 1;
    hPsData->headerCnt = 0;
  }

  /* nEnvelopes = 0 ? */
  if ((hPsData->noEnvCnt < MAX_NOENV_CNT) &&
      (similarIid(hPsData, psBands, nEnvelopes)) &&
      (similarIcc(hPsData, psBands, nEnvelopes)) &&
      /* Frankenstein: --ps-noenv-skip 0 forbids parameter-less PS frames.
       * Stock FDK may emit up to MAX_NOENV_CNT (10) consecutive frames carrying
       * NO stereo parameters at all whenever successive IID/ICC sets look
       * "similar" by its quantisation-step heuristic. On material with slowly
       * drifting panorama that can be heard as the stereo image briefly
       * collapsing / snapping back. 0 = always transmit parameters. */
      (g_franken.psNoEnvSkip != 0)) {
    hPsOut->nEnvelopes = nEnvelopes = 0;
    hPsData->noEnvCnt++;
  } else {
    hPsData->noEnvCnt = 0;
  }

  if (nEnvelopes > 0) {
    hPsOut->enableIID = hPsData->iidEnable;
    hPsOut->iidMode = getIIDMode(psBands, hPsData->iidQuantMode);

    hPsOut->enableICC = hPsData->iccEnable;
    hPsOut->iccMode = getICCMode(psBands, hPsData->iccQuantMode);

    hPsOut->enableIpdOpd = 0;
    hPsOut->frameClass = 0;
    hPsOut->nEnvelopes = nEnvelopes;

    for (env = 0; env < nEnvelopes; env++) {
      hPsOut->frameBorder[env] = envBorder[env + 1];
      hPsOut->deltaIID[env] = (PS_DELTA)hPsData->iidDiffMode[env];
      hPsOut->deltaICC[env] = (PS_DELTA)hPsData->iccDiffMode[env];
      for (band = 0; band < psBands; band++) {
        hPsOut->iid[env][band] = hPsData->iidIdx[env][band];
        hPsOut->icc[env][band] = hPsData->iccIdx[env][band];
      }
    }

    /* IPD: emitted only when --ps-ipd is on. OPD stays zero - the decoder does
     * not measure it but reconstructs it from a joint IPD/level/coherence model,
     * so a naive "phase of the downmix" value would be wrong; sending zeros is
     * the defined neutral. The extension is still well-formed either way. */
    hPsOut->enableIpdOpd = hPsData->ipdEnable;

    FDKmemclear(hPsOut->ipd,
                PS_MAX_ENVELOPES * PS_MAX_BANDS * sizeof(PS_DELTA));
    FDKmemclear(hPsOut->opd,
                PS_MAX_ENVELOPES * PS_MAX_BANDS * sizeof(PS_DELTA));
    /* filled in below when IPD/OPD are active */
    for (env = 0; env < PS_MAX_ENVELOPES; env++) {
      hPsOut->deltaIPD[env] = PS_DELTA_FREQ;
      hPsOut->deltaOPD[env] = PS_DELTA_FREQ;
    }
    FDKmemclear(hPsOut->ipdLast, PS_MAX_BANDS * sizeof(INT));
    FDKmemclear(hPsOut->opdLast, PS_MAX_BANDS * sizeof(INT));

    if (hPsData->ipdEnable) {
      for (env = 0; env < nEnvelopes; env++) {
        hPsOut->deltaIPD[env] = (PS_DELTA)hPsData->ipdDiffMode[env];
        hPsOut->deltaOPD[env] = (PS_DELTA)hPsData->opdDiffMode[env];
        for (band = 0; band < psBands; band++) {
          hPsOut->ipd[env][band] = hPsData->ipdIdx[env][band];
          hPsOut->opd[env][band] = hPsData->opdIdx[env][band];
        }
      }
      for (band = 0; band < PS_MAX_BANDS; band++) {
        hPsOut->ipdLast[band] = hPsData->ipdIdxLast[band];
        hPsOut->opdLast[band] = hPsData->opdIdxLast[band];
      }
    }

    for (band = 0; band < PS_MAX_BANDS; band++) {
      hPsOut->iidLast[band] = hPsData->iidIdxLast[band];
      hPsOut->iccLast[band] = hPsData->iccIdxLast[band];
    }

    /* save iids and iccs for differential time coding in the next frame */
    hPsData->nEnvelopesLast = nEnvelopes;
    hPsData->iidEnableLast = hPsData->iidEnable;
    hPsData->iccEnableLast = hPsData->iccEnable;
    hPsData->iidQuantModeLast = hPsData->iidQuantMode;
    hPsData->iccQuantModeLast = hPsData->iccQuantMode;
    for (i = 0; i < psBands; i++) {
      hPsData->iidIdxLast[i] = hPsData->iidIdx[nEnvelopes - 1][i];
      hPsData->iccIdxLast[i] = hPsData->iccIdx[nEnvelopes - 1][i];
    }
    /* Same roll-forward for IPD, and only now may ipdTimeCnt report that a
     * usable previous frame exists. */
    if (hPsData->ipdEnable) {
      for (i = 0; i < PS_MAX_BANDS; i++) {
        hPsData->ipdIdxLast[i] = hPsData->ipdIdx[nEnvelopes - 1][i];
        hPsData->opdIdxLast[i] = hPsData->opdIdx[nEnvelopes - 1][i];
      }
      hPsData->ipdEnableLast = hPsData->ipdEnable;
      hPsData->ipdTimeCnt = 0;
    }
  } /* Envelope > 0 */

  C_ALLOC_SCRATCH_END(pwrData, PS_PWR_DATA, 1)

  return error;
}
