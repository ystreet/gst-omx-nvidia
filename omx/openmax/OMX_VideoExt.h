/*
 * Copyright (c) 2010 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/** OMX_VideoExt.h - OpenMax IL version 1.1.2
 * The OMX_VideoExt header file contains extensions to the
 * definitions used by both the application and the component to
 * access video items.
 */

#ifndef OMX_VideoExt_h
#define OMX_VideoExt_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Each OMX header shall include all required header files to allow the
 * header to compile without errors.  The includes below are required
 * for this header file to compile successfully
 */
#include <OMX_Core.h>

/** NALU Formats */
typedef enum OMX_NALUFORMATSTYPE {
    OMX_NaluFormatStartCodes = 1,
    OMX_NaluFormatOneNaluPerBuffer = 2,
    OMX_NaluFormatOneByteInterleaveLength = 4,
    OMX_NaluFormatTwoByteInterleaveLength = 8,
    OMX_NaluFormatFourByteInterleaveLength = 16,
    OMX_NaluFormatCodingMax = 0x7FFFFFFF
} OMX_NALUFORMATSTYPE;

/** NAL Stream Format */
typedef struct OMX_NALSTREAMFORMATTYPE{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_NALUFORMATSTYPE eNaluFormat;
} OMX_NALSTREAMFORMATTYPE;

/** VP8 profiles */
typedef enum OMX_VIDEO_VP8PROFILETYPE {
    OMX_VIDEO_VP8ProfileMain = 0x01,
    OMX_VIDEO_VP8ProfileUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP8ProfileMax = 0x7FFFFFFF
} OMX_VIDEO_VP8PROFILETYPE;

/** VP8 levels */
typedef enum OMX_VIDEO_VP8LEVELTYPE {
    OMX_VIDEO_VP8Level_Version0 = 0x01,
    OMX_VIDEO_VP8Level_Version1 = 0x02,
    OMX_VIDEO_VP8Level_Version2 = 0x04,
    OMX_VIDEO_VP8Level_Version3 = 0x08,
    OMX_VIDEO_VP8LevelUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP8LevelMax = 0x7FFFFFFF
} OMX_VIDEO_VP8LEVELTYPE;

/** VP9 profiles */
typedef enum OMX_VIDEO_VP9PROFILETYPE {
    OMX_VIDEO_VP9Profile0       = 0x1,
    OMX_VIDEO_VP9Profile1       = 0x2,
    OMX_VIDEO_VP9Profile2       = 0x4,
    OMX_VIDEO_VP9Profile3       = 0x8,
    // HDR profiles also support passing HDR metadata
    OMX_VIDEO_VP9Profile2HDR    = 0x1000,
    OMX_VIDEO_VP9Profile3HDR    = 0x2000,
    OMX_VIDEO_VP9ProfileUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP9ProfileMax     = 0x7FFFFFFF
} OMX_VIDEO_VP9PROFILETYPE;

/** VP9 levels */
typedef enum OMX_VIDEO_VP9LEVELTYPE {
    OMX_VIDEO_VP9Level1       = 0x1,
    OMX_VIDEO_VP9Level11      = 0x2,
    OMX_VIDEO_VP9Level2       = 0x4,
    OMX_VIDEO_VP9Level21      = 0x8,
    OMX_VIDEO_VP9Level3       = 0x10,
    OMX_VIDEO_VP9Level31      = 0x20,
    OMX_VIDEO_VP9Level4       = 0x40,
    OMX_VIDEO_VP9Level41      = 0x80,
    OMX_VIDEO_VP9Level5       = 0x100,
    OMX_VIDEO_VP9Level51      = 0x200,
    OMX_VIDEO_VP9Level52      = 0x400,
    OMX_VIDEO_VP9Level6       = 0x800,
    OMX_VIDEO_VP9Level61      = 0x1000,
    OMX_VIDEO_VP9Level62      = 0x2000,
    OMX_VIDEO_VP9LevelUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP9LevelMax     = 0x7FFFFFFF
} OMX_VIDEO_VP9LEVELTYPE;

/** VP8 Param */
typedef struct OMX_VIDEO_PARAM_VP8TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_VIDEO_VP8PROFILETYPE eProfile;
    OMX_VIDEO_VP8LEVELTYPE eLevel;
    OMX_U32 nDCTPartitions;
    OMX_BOOL bErrorResilientMode;
} OMX_VIDEO_PARAM_VP8TYPE;

/** Structure for configuring VP8 reference frames */
typedef struct OMX_VIDEO_VP8REFERENCEFRAMETYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bPreviousFrameRefresh;
    OMX_BOOL bGoldenFrameRefresh;
    OMX_BOOL bAlternateFrameRefresh;
    OMX_BOOL bUsePreviousFrame;
    OMX_BOOL bUseGoldenFrame;
    OMX_BOOL bUseAlternateFrame;
} OMX_VIDEO_VP8REFERENCEFRAMETYPE;

/** Structure for querying VP8 reference frame type */
typedef struct OMX_VIDEO_VP8REFERENCEFRAMEINFOTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bIsIntraFrame;
    OMX_BOOL bIsGoldenOrAlternateFrame;
} OMX_VIDEO_VP8REFERENCEFRAMEINFOTYPE;

/** Maximum number of VP8 temporal layers */
#define OMX_VIDEO_ANDROID_MAXVP8TEMPORALLAYERS 3

/** VP8 temporal layer patterns */
typedef enum OMX_VIDEO_ANDROID_VPXTEMPORALLAYERPATTERNTYPE {
    OMX_VIDEO_VPXTemporalLayerPatternNone = 0,
    OMX_VIDEO_VPXTemporalLayerPatternWebRTC = 1,
    OMX_VIDEO_VPXTemporalLayerPatternMax = 0x7FFFFFFF
} OMX_VIDEO_ANDROID_VPXTEMPORALLAYERPATTERNTYPE;

/**
 * Android specific VP8 encoder params
 *
 * STRUCT MEMBERS:
 *  nSize                      : Size of the structure in bytes
 *  nVersion                   : OMX specification version information
 *  nPortIndex                 : Port that this structure applies to
 *  nKeyFrameInterval          : Key frame interval in frames
 *  eTemporalPattern           : Type of temporal layer pattern
 *  nTemporalLayerCount        : Number of temporal coding layers
 *  nTemporalLayerBitrateRatio : Bitrate ratio allocation between temporal
 *                               streams in percentage
 *  nMinQuantizer              : Minimum (best quality) quantizer
 *  nMaxQuantizer              : Maximum (worst quality) quantizer
 */
typedef struct OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nKeyFrameInterval;
    OMX_VIDEO_ANDROID_VPXTEMPORALLAYERPATTERNTYPE eTemporalPattern;
    OMX_U32 nTemporalLayerCount;
    OMX_U32 nTemporalLayerBitrateRatio[OMX_VIDEO_ANDROID_MAXVP8TEMPORALLAYERS];
    OMX_U32 nMinQuantizer;
    OMX_U32 nMaxQuantizer;
} OMX_VIDEO_PARAM_ANDROID_VP8ENCODERTYPE;

/** HEVC Profile enum type */
typedef enum OMX_VIDEO_HEVCPROFILETYPE {
    OMX_VIDEO_HEVCProfileUnknown            = 0x0,
    OMX_VIDEO_HEVCProfileMain               = 0x1,
    OMX_VIDEO_HEVCProfileMain10             = 0x2,
    OMX_VIDEO_HEVCProfileMainStillPicture   = 0x4,
    // Main10 profile with HDR SEI support.
    OMX_VIDEO_HEVCProfileMain10HDR10        = 0x1000,
    OMX_VIDEO_HEVCProfileMax                = 0x7FFFFFFF
} OMX_VIDEO_HEVCPROFILETYPE;

/** HEVC Level enum type */
typedef enum OMX_VIDEO_HEVCLEVELTYPE {
    OMX_VIDEO_HEVCLevelUnknown    = 0x0,
    OMX_VIDEO_HEVCMainTierLevel1  = 0x1,
    OMX_VIDEO_HEVCHighTierLevel1  = 0x2,
    OMX_VIDEO_HEVCMainTierLevel2  = 0x4,
    OMX_VIDEO_HEVCHighTierLevel2  = 0x8,
    OMX_VIDEO_HEVCMainTierLevel21 = 0x10,
    OMX_VIDEO_HEVCHighTierLevel21 = 0x20,
    OMX_VIDEO_HEVCMainTierLevel3  = 0x40,
    OMX_VIDEO_HEVCHighTierLevel3  = 0x80,
    OMX_VIDEO_HEVCMainTierLevel31 = 0x100,
    OMX_VIDEO_HEVCHighTierLevel31 = 0x200,
    OMX_VIDEO_HEVCMainTierLevel4  = 0x400,
    OMX_VIDEO_HEVCHighTierLevel4  = 0x800,
    OMX_VIDEO_HEVCMainTierLevel41 = 0x1000,
    OMX_VIDEO_HEVCHighTierLevel41 = 0x2000,
    OMX_VIDEO_HEVCMainTierLevel5  = 0x4000,
    OMX_VIDEO_HEVCHighTierLevel5  = 0x8000,
    OMX_VIDEO_HEVCMainTierLevel51 = 0x10000,
    OMX_VIDEO_HEVCHighTierLevel51 = 0x20000,
    OMX_VIDEO_HEVCMainTierLevel52 = 0x40000,
    OMX_VIDEO_HEVCHighTierLevel52 = 0x80000,
    OMX_VIDEO_HEVCMainTierLevel6  = 0x100000,
    OMX_VIDEO_HEVCHighTierLevel6  = 0x200000,
    OMX_VIDEO_HEVCMainTierLevel61 = 0x400000,
    OMX_VIDEO_HEVCHighTierLevel61 = 0x800000,
    OMX_VIDEO_HEVCMainTierLevel62 = 0x1000000,
    OMX_VIDEO_HEVCHighTierLevel62 = 0x2000000,
    OMX_VIDEO_HEVCLevelMax        = 0x7FFFFFFF
} OMX_VIDEO_HEVCLEVELTYPE;

/** Structure to define if temporal motion vector prediction should be used */
typedef enum OMX_VIDEO_HEVCTMVPTYPE {
    OMX_VIDEO_HEVCTMVPDisable           = 0x01,   /**< Disabled for all slices */
    OMX_VIDEO_HEVCTMVPEnable            = 0x02,   /**< Enabled for all slices  */
    OMX_VIDEO_HEVCTMVPDisableFirstFrame = 0x04,   /**< Disable only for first picture of each GOP size */
    OMX_VIDEO_HEVCTMVPMax               = 0x7FFFFFFF
} OMX_VIDEO_HEVCTMVPTYPE;

/** Structure to define if loop filter should be used */
typedef enum OMX_VIDEO_HEVCLOOPFILTERTYPE {
    OMX_VIDEO_HEVCLoopFilterEnable = 0,             /**< Enable HEVC Loop Filter  */
    OMX_VIDEO_HEVCLoopFilterDisable,                /**< Disable HEVC Loop Filter */
    OMX_VIDEO_HEVCLoopFilterDisableSliceBoundary,   /**< Disables HEVC Loop Filter on slice boundary */
    OMX_VIDEO_HEVCLoopFilterDisableTileBoundary,    /**< Disables HEVC Loop Filter on tile boundary  */
    OMX_VIDEO_HEVCLoopFilterDisablePCM,             /**< Disables HEVC Loop Filter for the reconstructed PCM samples */
    OMX_VIDEO_HEVCLoopFilterMax = 0x7FFFFFFF
} OMX_VIDEO_HEVCLOOPFILTERTYPE;

/**
 * HEVC params
 *
 * STRUCT MEMBERS:
 *  nSize                          : Size of the structure in bytes
 *  nVersion                       : OMX specification version information
 *  nPortIndex                     : Port that this structure applies to
 *  nPFrames                       : Number of P frames between each I frame
 *  nBFrames                       : Number of B frames between each I frame
 *  eProfile                       : HEVC profile(s) to use
 *  eLevel                         : HEVC level(s) to use
 *  nRefFrames                     : Number of reference frames to use for inter-
 *                                   motion search
 *  nNumLayers                     : Number of layers in the bitstream
 *  nNumSubLayers                  : Number of temporal sub-layers in the
 *                                   bitstream (range [0, 6])
 *  bEnableSCP                     : Enable/disable separate plane coding for
 *                                   YUV 4:4:4 inputs
 *  bEnableScalingList             : Enable/disable scaling process for transform
 *                                   coefficients
 *  bEnableAMP                     : Enable/disable asymmetric motion partitions
 *  bEnablePCM                     : Enable/disable PCM data in the bitstream
 *  bEnableSIS                     : Enable/disable strong intra smoothing filtering
 *  bWeightedPPrediction           : Enable/disable weighted prediction applied
 *                                   to P slices
 *  bWeightedBPrediction           : Enable/disable weighted prediction applied
 *                                   to B slices
 *  bEnableTiles                   : Enable/disable multiple tiles in each picture
 *  bEnableECSync                  : Enable/disable entropy coding synchronization
 *  bEnableUniformSpacing          : Enable/disable uniform spacing of tile column
 *                                   and row boundaries across the picture
 *  bEnableSAO                     : Enable/disable sample adaptive offset filter
 *  bEnableConstrainedIntraPred    : Enable/disable constrained intra prediction
 *  bEnableTransquantBypass        : Enable/disable ability to bypass transform,
 *                                   quantization and filtering
 *  eTMVPMode                      : Control temporal motion vector prediction
 *  bEnableTransformSkip           : Enable/disable transform-skipping for
 *                                   4x4 TUs
 *  eLoopFilterType                : Enable/disable HEVC loop filter
 *  nMaxTemporalId                 : Maximum temporal id of NAL units
 */
typedef struct OMX_VIDEO_PARAM_HEVCTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nPFrames;
    OMX_U32 nBFrames;
    OMX_VIDEO_HEVCPROFILETYPE eProfile;
    OMX_VIDEO_HEVCLEVELTYPE eLevel;
    OMX_U32 nKeyFrameInterval;
    OMX_U32 nRefFrames;
    OMX_U32 nNumLayers;
    OMX_U32 nNumSubLayers;
    OMX_BOOL bEnableSCP;
    OMX_BOOL bEnableScalingList;
    OMX_BOOL bEnableAMP;
    OMX_BOOL bEnablePCM;
    OMX_BOOL bEnableSIS;
    OMX_BOOL bWeightedPPrediction;
    OMX_BOOL bWeightedBPrediction;
    OMX_BOOL bEnableTiles;
    OMX_BOOL bEnableECSync;
    OMX_BOOL bEnableUniformSpacing;
    OMX_BOOL bEnableSAO;
    OMX_BOOL bEnableConstrainedIntraPred;
    OMX_BOOL bEnableTransquantBypass;
    OMX_VIDEO_HEVCTMVPTYPE eTMVPMode;
    OMX_BOOL bEnableTransformSkip;
    OMX_VIDEO_HEVCLOOPFILTERTYPE eLoopFilterMode;
    OMX_U32 nMaxTemporalId;
} OMX_VIDEO_PARAM_HEVCTYPE;

/** Structure to define if dependent slice segments should be used */
typedef struct OMX_VIDEO_SLICESEGMENTSTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bDepedentSegments;
    OMX_BOOL bEnableLoopFilterAcrossSlices;
} OMX_VIDEO_SLICESEGMENTSTYPE;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OMX_VideoExt_h */
/* File EOF */
