/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <OMX_IndexExt.h>
#include "gstomxvp8enc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_vp8_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_vp8_enc_debug_category

/* prototypes */
static gboolean gst_omx_vp8_enc_set_format (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstCaps *gst_omx_vp8_enc_get_caps (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);

enum
{
  PROP_0
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_vp8_enc_debug_category, "omxvp8enc", 0, \
      "debug category for gst-omx video encoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXVP8Enc, gst_omx_vp8_enc,
    GST_TYPE_OMX_VIDEO_ENC, DEBUG_INIT);

static void
gst_omx_vp8_enc_class_init (GstOMXVP8EncClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOMXVideoEncClass *videoenc_class = GST_OMX_VIDEO_ENC_CLASS (klass);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_vp8_enc_set_format);
  videoenc_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_vp8_enc_get_caps);

  videoenc_class->cdata.default_src_template_caps = "video/x-vp8, "
      "width=(int) [ 16, 4096 ], " "height=(int) [ 16, 4096 ]";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX VP8 Video Encoder",
      "Codec/Encoder/Video",
      "Encode VP8 video streams", "Jitendra Kumar <jitendrak@nvidia.com>");

  gst_omx_set_default_role (&videoenc_class->cdata, "video_encoder.vp8");
}

static void
gst_omx_vp8_enc_init (GstOMXVP8Enc * self)
{
}

static gboolean
gst_omx_vp8_enc_set_format (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXVP8Enc *self = GST_OMX_VP8_ENC (enc);
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_ERRORTYPE err;

  gst_omx_port_get_port_definition (GST_OMX_VIDEO_ENC (self)->enc_out_port,
      &port_def);
  port_def.format.video.eCompressionFormat = OMX_VIDEO_CodingAutoDetect;
  err =
      gst_omx_port_update_port_definition (GST_OMX_VIDEO_ENC
      (self)->enc_out_port, &port_def);
  if (err != OMX_ErrorNone)
    return FALSE;

  return TRUE;
}

static GstCaps *
gst_omx_vp8_enc_get_caps (GstOMXVideoEnc * self, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstCaps *caps;
  OMX_ERRORTYPE err;
  OMX_VIDEO_PARAM_VP8TYPE param;
  gint profile = 0;

  caps = gst_caps_new_empty_simple ("video/x-vp8");

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = self->enc_out_port->index;
  err = gst_omx_component_get_parameter (self->enc, OMX_IndexParamVideoVp8, &param);
  if (err != OMX_ErrorNone && err != OMX_ErrorUnsupportedIndex
      && err != OMX_ErrorNotImplemented)
     GST_WARNING_OBJECT (self, "Coudn't get parameter for OMX_IndexParamVideoVp8");

  if (err == OMX_ErrorNone) {
    switch (param.eLevel) {
      case OMX_VIDEO_VP8Level_Version0:
        profile = 0;
        break;
      case OMX_VIDEO_VP8Level_Version1:
        profile = 1;
        break;
      case OMX_VIDEO_VP8Level_Version2:
        profile = 2;
        break;
      case OMX_VIDEO_VP8Level_Version3:
        profile = 3;
        break;
      default:
        profile = 0;
        break;
    }

    gst_caps_set_simple (caps, "profile", G_TYPE_INT, profile, NULL);
  }


  return caps;
}
