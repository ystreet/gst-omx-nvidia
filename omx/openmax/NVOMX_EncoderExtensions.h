/* Copyright (c) 2010-2017 NVIDIA Corporation.  All rights reserved.
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
 */

/**
 * @file
 * <b>NVIDIA Tegra: OpenMAX Encoder Extension Interface</b>
 *
 */

/**
 * @defgroup nv_omx_il_encoder Encoder
 *
 * This is the NVIDIA OpenMAX encoder class extensions interface.
 *
 * These extensions include ultra low power (ULP) mode, video de-interlacing, JPEG EXIF info,
 * thumbnail generation and more.
 *
 * @ingroup nvomx_encoder_extension
 * @{
 */

#ifndef NVOMX_EncoderExtensions_h_
#define NVOMX_EncoderExtensions_h_


#define MAX_EXIF_STRING_IN_BYTES                   (200)
#define MAX_GPS_STRING_IN_BYTES                    (32)
#define GPS_DATESTAMP_LENGTH                       (11)
#define MAX_INTEROP_STRING_IN_BYTES                (32)
#define MAX_GPS_PROCESSING_METHOD_IN_BYTES         (40)
#define MAX_REF_FRAMES                             (8)
#define MAX_ROI_REGIONS                            (8)
#define MV_BUFFER_HEADER                           (0xFFFEFDFC)
/* General encoder extensions */

/* Audio encoder extensions */

/* Video encoder extensions */

/** Config extension index to set peak bitrate in VBR(veriable bitrate) mode.
 */
#define NVX_INDEX_CONFIG_VIDEO_PEAK_BITRATE  "OMX.Nvidia.index.config.video.peak.bitrate"

/* JPEG encoder extensions */

/** Config extension index to set EXIF information (image encoder classes only).
 *  See ::NVX_CONFIG_ENCODEEXIFINFO
 */
#define NVX_INDEX_CONFIG_ENCODEEXIFINFO "OMX.Nvidia.index.config.encodeexifinfo"
/** Holds information to set EXIF information. */
typedef struct NVX_CONFIG_ENCODEEXIFINFO
{
    OMX_U32 nSize;                          /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;               /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                     /**< Port that this struct applies to */

    OMX_S8  ImageDescription[MAX_EXIF_STRING_IN_BYTES];     /**< String describing image */
    OMX_S8  Make[MAX_EXIF_STRING_IN_BYTES];                 /**< String of image make */
    OMX_S8  Model[MAX_EXIF_STRING_IN_BYTES];                /**< String of image model */
    OMX_S8  Copyright[MAX_EXIF_STRING_IN_BYTES];            /**< String of image copyright */
    OMX_S8  Artist[MAX_EXIF_STRING_IN_BYTES];               /**< String of image artist */
    OMX_S8  Software[MAX_EXIF_STRING_IN_BYTES];             /**< String of software creating image */
    OMX_S8  DateTime[MAX_EXIF_STRING_IN_BYTES];             /**< String of date and time */
    OMX_S8  DateTimeOriginal[MAX_EXIF_STRING_IN_BYTES];     /**< String of original date and time */
    OMX_S8  DateTimeDigitized[MAX_EXIF_STRING_IN_BYTES];    /**< String of digitized date and time */
    OMX_U16 filesource;                                     /**< File source */
    OMX_S8  UserComment[MAX_EXIF_STRING_IN_BYTES];          /**< String user comments */
    OMX_U16 Orientation;                                    /**< Orientation of the image: 0,90,180,270*/
} NVX_CONFIG_ENCODEEXIFINFO;

/** OMX  Encode GpsBitMap Type
  * Enable and disable the exif gps fields individually
  * See also NVX_CONFIG_ENCODEGPSINFO.GPSBitMapInfo
  */
typedef enum OMX_ENCODE_GPSBITMAPTYPE {
    OMX_ENCODE_GPSBitMapLatitudeRef =      0x01,
    OMX_ENCODE_GPSBitMapLongitudeRef =     0x02,
    OMX_ENCODE_GPSBitMapAltitudeRef =      0x04,
    OMX_ENCODE_GPSBitMapTimeStamp =        0x08,
    OMX_ENCODE_GPSBitMapSatellites =       0x10,
    OMX_ENCODE_GPSBitMapStatus =           0x20,
    OMX_ENCODE_GPSBitMapMeasureMode =      0x40,
    OMX_ENCODE_GPSBitMapDOP =              0x80,
    OMX_ENCODE_GPSBitMapImgDirectionRef =  0x100,
    OMX_ENCODE_GPSBitMapMapDatum =         0x200,
    OMX_ENCODE_GPSBitMapProcessingMethod = 0x400,
    OMX_ENCODE_GPSBitMapDateStamp =        0x800,
    OMX_ENCODE_GPSBitMapMax =              0x7FFFFFFF
} OMX_ENCODE_GPSBITMAPTYPE;



/** Config extension index to set GPS information (image encoder classes only).
 *  See ::NVX_CONFIG_ENCODEGPSINFO
 */
#define NVX_INDEX_CONFIG_ENCODEGPSINFO "OMX.Nvidia.index.config.encodegpsinfo"
/** Holds information to set GPS information. */
typedef struct NVX_CONFIG_ENCODEGPSINFO
{
    OMX_U32 nSize;                          /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;               /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                     /**< Port that this struct applies to */

    OMX_U32 GPSBitMapInfo;
    OMX_U32 GPSVersionID;                   /**< Version identifier */
    OMX_S8  GPSLatitudeRef[2];              /**< Latitude reference */
    OMX_U32 GPSLatitudeNumerator[3];        /**< Latitude numerator */
    OMX_U32 GPSLatitudeDenominator[3];      /**< Latitude denominator */
    OMX_S8  GPSLongitudeRef[2];             /**< Longitude reference */
    OMX_U32 GPSLongitudeNumerator[3];       /**< Longitude numerator */
    OMX_U32 GPSLongitudeDenominator[3];     /**< Longitude denominator */
    OMX_U8  GPSAltitudeRef;                 /**< Altitude reference */
    OMX_U32 GPSAltitudeNumerator;           /**< Altitude numerator */
    OMX_U32 GPSAltitudeDenominator;         /**< Altitude denominator */
    OMX_U32 GPSTimeStampNumerator[3];       /**< Timestamp numerator */
    OMX_U32 GPSTimeStampDenominator[3];     /**< Timestamp denominator */
    OMX_S8  GPSSatellites[MAX_GPS_STRING_IN_BYTES];
    OMX_S8  GPSStatus[2];
    OMX_S8  GPSMeasureMode[2];
    OMX_U32 GPSDOPNumerator;
    OMX_U32 GPSDOPDenominator;
    OMX_S8  GPSImgDirectionRef[2];
    OMX_U32 GPSImgDirectionNumerator;
    OMX_U32 GPSImgDirectionDenominator;
    OMX_S8  GPSMapDatum[MAX_GPS_STRING_IN_BYTES];
    OMX_S8  GPSDateStamp[GPS_DATESTAMP_LENGTH];
    OMX_S8  GPSProcessingMethod[MAX_GPS_PROCESSING_METHOD_IN_BYTES];
} NVX_CONFIG_ENCODEGPSINFO;


/** Config extension index to set Interoperability IFD information (image encoder classes only).
 *  See ::NVX_CONFIG_ENCODE_INTEROPERABILITYINFO
 */
#define NVX_INDEX_CONFIG_ENCODE_INTEROPINFO "OMX.Nvidia.index.config.encodeinteropinfo"
/** Holds information to set GPS information. */

typedef struct NVX_CONFIG_ENCODE_INTEROPINFO
{
    OMX_U32 nSize;                          /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;               /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                     /**< Port that this struct applies to */

    OMX_S8 Index[MAX_INTEROP_STRING_IN_BYTES];
} NVX_CONFIG_ENCODE_INTEROPINFO;

/** Config extension to mirror ::OMX_IndexParamQuantizationTable.
 * See: ::OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE
 */
#define NVX_INDEX_CONFIG_ENCODE_QUANTIZATIONTABLE \
    "OMX.NVidia.index.config.encodequantizationtable"

/** Config extension index to set thumbnail quality factor for JPEG encoder (image encoder classes only).
 */
#define NVX_INDEX_CONFIG_THUMBNAILQF  "OMX.Nvidia.index.config.thumbnailqf"

/** Param extension index to set/get quantization table (luma and chroma) for thumbnail image.
 * (jpeg image encoder class only)
 * See OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE
 */
#define NVX_INDEX_PARAM_THUMBNAILIMAGEQUANTTABLE "OMX.Nvidia.index.param.thumbnailquanttable"

typedef enum NVX_VIDEO_VP9PROFILETYPE {
    NVX_VIDEO_VP9Profile0                   = 0x01,
    NVX_VIDEO_VP9Profile1                   = 0x02,
    NVX_VIDEO_VP9Profile2                   = 0x04,
    NVX_VIDEO_VP9Profile3                   = 0x08,
    NVX_VIDEO_VP9ProfileMax                 = 0x7FFFFFFF
} NVX_VIDEO_VP9PROFILETYPE;

/** Config extension index to set VP9 encoding parameters
 */
#define NVX_INDEX_PARAM_VP9TYPE "OMX.Nvidia.index.param.vp9type"

typedef struct NVX_VIDEO_PARAM_VP9TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    NVX_VIDEO_VP9PROFILETYPE eProfile;
    OMX_U32 nDCTPartitions;
    OMX_BOOL bErrorResilientMode;
    OMX_U32 nPFrames;
    OMX_U32 filter_level;
    OMX_U32 sharpness_level;
} NVX_VIDEO_PARAM_VP9TYPE;

#define NVX_VIDEO_MAXVPXTEMPORALLAYERS 3

/* enum for temporal layer pattern
 * NVX_INDEX_PARAM_VPXTEMPORALLAYERPATTERNTYPE
 */
typedef enum NVX_VIDEO_VPXTEMPORALLAYERPATTERNTYPE {
    NVX_VIDEO_VPXTemporalLayerPatternNone=0,
    NVX_VIDEO_VPXTemporalLayerPatternWebRTC=1,
    NVX_VIDEO_VPXTemporalLayerPatternMax=0x7FFFFFFF
} NVX_VIDEO_VPXTEMPORALLAYERPATTERNTYPE;

/* Config extension to set parameters for vp9 encoder
 * sets temporal pattern, qp, keyframe interval and bitrate ratio
 * NVX_INDEX_PARAM_VP9ENCODERTYPE
 */
#define NVX_INDEX_PARAM_VP9ENCODERTYPE "OMX.Nvidia.index.param.vp9encodertype"

typedef struct NVX_VIDEO_PARAM_VP9ENCODERTYPE {
    OMX_U32 nSize;                /* Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;     /* version information */
    OMX_U32 nPortIndex;           /* Port that this struct applies to */
    OMX_U32 nKeyFrameInterval;    /* key frame interval in frames */
    NVX_VIDEO_VPXTEMPORALLAYERPATTERNTYPE eTemporalPattern;
                                  /* temporal pattern type */
    OMX_U32 nTemporalLayerCount;  /* number of layers */
    OMX_U32 nTemporalLayerBitrateRatio[NVX_VIDEO_MAXVPXTEMPORALLAYERS];
                                  /* Bit rate ratio for different layers */
    OMX_U32 nMinQuantizer;        /* minimum quanitzer */
    OMX_U32 nMaxQuantizer;        /* maximum quaaniter */
} NVX_VIDEO_PARAM_VP9ENCODERTYPE;

/* OMX extension index to configure encoder to send - P frame with all skipped MBs */
/**< reference: NVX_INDEX_PARAM_VIDENC_GEN_SKIP_MB_FRAMES
 * Use OMX_CONFIG_BOOLEANTYPE
 */
#define NVX_INDEX_PARAM_VIDENC_SKIP_FRAME "OMX.Nvidia.index.param.videncskipframe"

typedef struct NVOMX_FrameProp {
    OMX_U32 nFrameId;           // unique Id
    OMX_BOOL bLTRefFrame;       // Long Term Ref Flag. Note: a previously short term may be turned to long term with this flag
} NVOMX_FrameProp;

typedef struct NVX_VIDEO_ENC_INPUT_H264_SPECIFIC_DATA {
    OMX_U32 nFrameId;                               // unique Id of current frame
    OMX_BOOL bRefFrame;                             // current frame referenced or non-referenced
    OMX_BOOL bLTRefFrame;                           // current frame long Term Ref Flag
    OMX_U32 nMaxRefFrames;                          // Max Number of reference frames to use for inter-motion search
    OMX_U32 nActiveRefFrames;                       // # of valid entries in RPS, 0 means IDR
    OMX_U32 nCurrentRefFrameId;                     // frame id of reference frame to be used for motion search, ignored for IDR
    NVOMX_FrameProp RPSList[MAX_REF_FRAMES];        // RPS
} NVX_VIDEO_ENC_INPUT_H264_SPECIFIC_DATA;

typedef struct NVX_VIDEO_ENC_INPUT_HEVC_SPECIFIC_DATA {
    OMX_U32 nFrameId;                               // unique Id of current frame
    OMX_BOOL bRefFrame;                             // current frame referenced or non-referenced
    OMX_BOOL bLTRefFrame;                           // current frame long Term Ref Flag
    OMX_U32 nMaxRefFrames;                          // Max Number of reference frames to use for inter-motion search
    OMX_U32 nActiveRefFrames;                       // # of valid entries in RPS, 0 means IDR
    OMX_U32 nCurrentRefFrameId;                     // frame id of reference frame to be used for motion search, ignored for IDR
    NVOMX_FrameProp RPSList[MAX_REF_FRAMES];        // RPS
} NVX_VIDEO_ENC_INPUT_HEVC_SPECIFIC_DATA;

typedef struct NVOMX_Rect {
    OMX_U32 left;   /**< Left pixel of frame */
    OMX_U32 top;    /**< Top pixel of frame */
    OMX_U32 right;  /**< Right pixel of frame */
    OMX_U32 bottom; /**< Bottom pixel of frame */
}NVOMX_Rect;

typedef struct NvOMX_ROIParams
{
    NVOMX_Rect  ROIRect;        //Region of interest rectangle
    OMX_S32     QPdelta;        // QP delta for Region
} NvOMX_ROIParams;

typedef enum NVX_VIDEO_ENC_INPUT_FRAME_PARAMS_FLAGS {
    NVX_VIDEO_ENC_INPUT_PARAMS_FLAG_GENERATE_CRC = 0x1,     /*To generate recon CRC*/
    NVX_VIDEO_ENC_INPUT_PARAMS_FLAG_USE_GDR      = 0x2      /*To enable cyclic Intra refresh for speified frames*/
    /*Other flags for future requirements*/
} NVX_VIDEO_ENC_INPUT_FRAME_PARAMS_FLAGS;

typedef struct NVX_VIDEO_ENC_INPUT_EXTRA_DATA {
    OMX_U32 nSize;                /* Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;     /* version information */
    OMX_U32 nEncodeParamsFlag;   /* Flag to indicate type of extra data present */
    /* We can add encoder configuration parameter here which can be altered on a frame level */
    NVOMX_Rect nReconCRCRect;
    /* parameter for rate control */
    OMX_U32 nTargetFrameBits; /**< Target frame Bits to use for current frame */
    OMX_U32 nFrameQP; /**< Start frame QP value to use for current frame */
    OMX_U32 nFrameMinQp; /**< Minimum QP value to use for current frame */
    OMX_U32 nFrameMaxQp; /**< Maximum QP value to use for current frame */
    OMX_U32 nMaxQPDeviation; /**< MaxQP deviation to use for current frame */
    OMX_U32 nNumROIRegions;  /** Num Of ROI Region  **/
    NvOMX_ROIParams nROIParams[MAX_ROI_REGIONS];        //ROI params
    /* Parameter for GDR (Intra refresh) */
    OMX_U32 nGDRFrames;     /** < Cyclic Intra refresh for spefied number of frames */
    /* Any other frame level parameters */
    union {
        NVX_VIDEO_ENC_INPUT_H264_SPECIFIC_DATA h264Data;
        NVX_VIDEO_ENC_INPUT_HEVC_SPECIFIC_DATA hevcData;
    }codecData;
} NVX_VIDEO_ENC_INPUT_EXTRA_DATA;

typedef struct NVOMX_FrameFullProp {
    OMX_U32 nFrameId;           // unique Id
    OMX_BOOL bIdrFrame;         // Is an IDR
    OMX_BOOL bLTRefFrame;       // Long Term Ref Flag
    OMX_U32 nPictureOrderCnt;   // POC
    OMX_U32 nFrameNum;          // FrameNum
    OMX_U32 nLTRFrameIdx;       // LongTermFrameIdx of a picture
} NVOMX_FrameFullProp;

typedef struct NVX_VIDEO_ENC_OUTPUT_H264_SPECIFIC_DATA {
    OMX_BOOL bStatus;                             // Error Status . Success or Failure
    OMX_U32 nCurrentRefFrameId;                   // frame id of reference frame to be used for motion search, ignored for IDR
    OMX_U32 nActiveRefFrames;                     // # of valid entries in RPS
    NVOMX_FrameFullProp RPSList[MAX_REF_FRAMES];  // RPS List including most recent frame if it is reference frame
} NVX_VIDEO_ENC_OUTPUT_H264_SPECIFIC_DATA;

typedef struct NVX_VIDEO_ENC_OUTPUT_HEVC_SPECIFIC_DATA {
    OMX_BOOL bStatus;                             // Error Status . Success or Failure
    OMX_U32 nCurrentRefFrameId;                   // frame id of reference frame to be used for motion search, ignored for IDR
    OMX_U32 nActiveRefFrames;                     // # of valid entries in RPS
    NVOMX_FrameFullProp RPSList[MAX_REF_FRAMES];  // RPS List including most recent frame if it is reference frame
} NVX_VIDEO_ENC_OUTPUT_HEVC_SPECIFIC_DATA;

typedef struct NVX_VIDEO_ENC_OUTPUT_EXTRA_DATA {
    OMX_U32 nSize;                /* Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;     /* version information */
    /* For reconstructed surface CRC */
    OMX_BOOL bValidReconCRC;     /**< This is set to true if valid recon CRC is present */
    OMX_U32 nReconCRC_Y;         /**< recon CRC for Y component for given recon retangle */
    OMX_U32 nReconCRC_U;         /**< recon CRC for U component for given recon retangle */
    OMX_U32 nReconCRC_V;         /**< recon CRC for V component for given recon retangle */
    /* Rate control used parameters */
    OMX_U32 nAvgFrameQP;         /**< Average frame QP for current encoded frame */
    OMX_U32 nEncodedFrameBits;   /**< Encoded frame size in Bits for current frame */
    OMX_U32 nFrameMinQP;         /**< Minimum QP used for current frame */
    OMX_U32 nFrameMaxQP;         /**< Maximum QP used for current frame */
    union {
        NVX_VIDEO_ENC_OUTPUT_H264_SPECIFIC_DATA h264Data;
        NVX_VIDEO_ENC_OUTPUT_HEVC_SPECIFIC_DATA hevcData;
    }codecData;
    OMX_BOOL bMVbufferdump;
    OMX_U32  data[1];     /* Supporting data hint  */
} NVX_VIDEO_ENC_OUTPUT_EXTRA_DATA;

/* MV Buffer contain MV buffer header and one MotionVector entry per block size */
typedef struct {
    OMX_S32 mv_x   : 16;
    OMX_S32 mv_y   : 16;
} MotionVector;

typedef struct
{
    OMX_U32 MagicNum;
    OMX_U32 buffersize;
    OMX_U16 blocksize;
    OMX_U16 width_in_blocksize;
    OMX_U16 height_in_blocksize;
    OMX_U16 reserved;
}MotionVectorHeader;

#endif
/** @} */
/* File EOF */


