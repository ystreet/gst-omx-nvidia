/* Copyright (c) 2015-2016 NVIDIA Corporation.  All rights reserved.
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

#ifndef NVOMX_TNR_h_
#define NVOMX_TNR_h_

#define NVX_INDEX_CONFIG_TNR "OMX.Nvidia.index.config.tnr"

typedef enum {
    YUV_RESCALE_NONE = 0,
    YUV_RESCALE_STD_TO_EXT = 1,
    YUV_RESCALE_EXT_TO_STD = 2
} YUVRescale;

typedef struct NVX_CONFIG_TNR
{
    OMX_U32 nSize;                          /**< Size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< NVX extensions specification version information */
    OMX_U32 nPortIndex;                     /**< Port that this struct applies to */
    NVX_F32 tnrStrength;                     /**< Port that this struct applies to */
    OMX_U32 tnrAlgorithm;                     /**< Port that this struct applies to */
    YUVRescale nYuvRescale;                     /**< Port that this struct applies to */

} NVX_CONFIG_TNR;

#endif
