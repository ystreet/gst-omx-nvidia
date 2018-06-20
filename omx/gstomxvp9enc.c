/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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
#include "gstomxvp9enc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_vp9_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_vp9_enc_debug_category

/* prototypes */
static gboolean gst_omx_vp9_enc_set_format (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstCaps *gst_omx_vp9_enc_get_caps (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);

enum
{
  PROP_0
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_vp9_enc_debug_category, "omxvp9enc", 0, \
      "debug category for gst-omx video encoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXVP9Enc, gst_omx_vp9_enc,
    GST_TYPE_OMX_VIDEO_ENC, DEBUG_INIT);

static void
gst_omx_vp9_enc_class_init (GstOMXVP9EncClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOMXVideoEncClass *videoenc_class = GST_OMX_VIDEO_ENC_CLASS (klass);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_vp9_enc_set_format);
  videoenc_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_vp9_enc_get_caps);

  videoenc_class->cdata.default_src_template_caps = "video/x-vp9, "
      "width=(int) [ 16, 4096 ], " "height=(int) [ 16, 4096 ]";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX VP9 Video Encoder",
      "Codec/Encoder/Video",
      "Encode VP9 video streams", "Jitendra Kumar <jitendrak@nvidia.com>");

  gst_omx_set_default_role (&videoenc_class->cdata, "video_encoder.vp9");
}

static void
gst_omx_vp9_enc_init (GstOMXVP9Enc * self)
{
}

static gboolean
gst_omx_vp9_enc_set_format (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXVP9Enc *self = GST_OMX_VP9_ENC (enc);
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_ERRORTYPE err;

  gst_omx_port_get_port_definition (GST_OMX_VIDEO_ENC (self)->enc_out_port,
      &port_def);
  port_def.format.video.eCompressionFormat = NVX_VIDEO_CodingVP9;
  err =
      gst_omx_port_update_port_definition (GST_OMX_VIDEO_ENC
      (self)->enc_out_port, &port_def);
  if (err != OMX_ErrorNone)
    return FALSE;

  return TRUE;
}

static GstCaps *
gst_omx_vp9_enc_get_caps (GstOMXVideoEnc * self, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstCaps *caps;
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE err;
  NVX_VIDEO_PARAM_VP9TYPE param;
  gint profile = 0;

  caps = gst_caps_new_empty_simple ("video/x-vp9");
  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = self->enc_out_port->index;

  err = gst_omx_component_get_index (self->enc,(char* )NVX_INDEX_PARAM_VP9TYPE, &eIndex);

  if (err == OMX_ErrorNone) {
    err = gst_omx_component_get_parameter (self->enc, eIndex, &param);
    if (err != OMX_ErrorNone && err != OMX_ErrorUnsupportedIndex
        && err != OMX_ErrorNotImplemented)
       GST_WARNING_OBJECT (self, "Coudn't get VP9 encoder profile");

    if (err == OMX_ErrorNone) {
      switch (param.eProfile) {
        case NVX_VIDEO_VP9Profile0:
          profile = 0;
          break;
        case NVX_VIDEO_VP9Profile1:
          profile = 1;
          break;
        case NVX_VIDEO_VP9Profile2:
          profile = 2;
          break;
        case NVX_VIDEO_VP9Profile3:
          profile = 3;
          break;
        default:
          profile = 0;
          break;
      }
  }

    gst_caps_set_simple (caps, "profile", G_TYPE_INT, profile, NULL);
  }
  else
    GST_WARNING_OBJECT (self, "Coudn't get extension index for %s",
          (char *) NVX_INDEX_PARAM_VP9TYPE);

  return caps;
}
