/* GStreamer
 * Copyright (c) 2015-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __GST_OMX_H265_ENC_H__
#define __GST_OMX_H265_ENC_H__

#include <gst/gst.h>
#include "gstomxvideoenc.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_H265_ENC \
  (gst_omx_h265_enc_get_type())
#define GST_OMX_H265_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_H265_ENC,GstOMXH265Enc))
#define GST_OMX_H265_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_H265_ENC,GstOMXH265EncClass))
#define GST_OMX_H265_ENC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_OMX_H265_ENC,GstOMXH265EncClass))
#define GST_IS_OMX_H265_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_H265_ENC))
#define GST_IS_OMX_H265_ENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_H265_ENC))
typedef struct _GstOMXH265Enc GstOMXH265Enc;
typedef struct _GstOMXH265EncClass GstOMXH265EncClass;

typedef enum
{
  H265_HVC1,
  H265_BTS
} h265_sf;

struct _GstOMXH265Enc
{
  GstOMXVideoEnc parent;
  h265_sf stream_format;
  gboolean insert_sps_pps;
  glong slice_header_spacing;
  gboolean insert_aud;
  gboolean insert_vui;
};

struct _GstOMXH265EncClass
{
  GstOMXVideoEncClass parent_class;
};

GType gst_omx_h265_enc_get_type (void);

G_END_DECLS
#endif /* __GST_OMX_H265_ENC_H__ */
