/* GStreamer
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstomxh265dec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_h265_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_h265_dec_debug_category

/* prototypes */
static gboolean gst_omx_h265_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);
static gboolean gst_omx_h265_dec_set_format (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);
static void gst_omx_h265_dec_loop (GstOMXBuffer * buf);

enum
{
  PROP_0,
  PROP_ENABLE_FRAME_TYPE_REPORTING
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_h265_dec_debug_category, "omxh265dec", 0, \
      "debug category for gst-omx video decoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXH265Dec, gst_omx_h265_dec,
    GST_TYPE_OMX_VIDEO_DEC, DEBUG_INIT);

static void
gst_omx_h265_dec_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOMXVideoDec *self = GST_OMX_VIDEO_DEC (object);

  switch (prop_id) {
    case PROP_ENABLE_FRAME_TYPE_REPORTING:
      self->enable_frame_type_reporting = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_h265_dec_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOMXVideoDec *self = GST_OMX_VIDEO_DEC (object);

  switch (prop_id) {
    case PROP_ENABLE_FRAME_TYPE_REPORTING:
      g_value_set_boolean (value, self->enable_frame_type_reporting);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_h265_dec_class_init (GstOMXH265DecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstOMXVideoDecClass *videodec_class = GST_OMX_VIDEO_DEC_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  videodec_class->is_format_change =
      GST_DEBUG_FUNCPTR (gst_omx_h265_dec_is_format_change);
  videodec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_h265_dec_set_format);
  videodec_class->video_dec_loop = GST_DEBUG_FUNCPTR (gst_omx_h265_dec_loop);

  gobject_class->set_property = gst_omx_h265_dec_set_property;
  gobject_class->get_property = gst_omx_h265_dec_get_property;

  videodec_class->cdata.default_sink_template_caps = "video/x-h265, "
      "parsed=(boolean) true, "
      "alignment=(string) au, "
      "stream-format=(string) byte-stream, "
      "width=(int) [1,MAX], " "height=(int) [1,MAX]";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX H.265 Video Decoder",
      "Codec/Decoder/Video",
      "Decode H.265 video streams",
      "Sanket Kothari <skothari@nvidia.com>");

  g_object_class_install_property (gobject_class, PROP_ENABLE_FRAME_TYPE_REPORTING,
      g_param_spec_boolean ("enable-frame-type-reporting",
          "enable-frame-type-reporting",
          "Set to enable frame type reporting",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_omx_set_default_role (&videodec_class->cdata, "video_decoder.hevc");
}

static void
gst_omx_h265_dec_init (GstOMXH265Dec * self)
{

}

static gboolean
gst_omx_h265_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state)
{
  return FALSE;
}


static gboolean
gst_omx_h265_dec_set_format (GstOMXVideoDec * dec, GstOMXPort * port,
    GstVideoCodecState * state)
{
  gboolean ret;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;

  gst_omx_port_get_port_definition (port, &port_def);
  port_def.format.video.eCompressionFormat = NVX_VIDEO_CodingHEVC;

  ret = gst_omx_port_update_port_definition (port, &port_def) == OMX_ErrorNone;

  return ret;
}

static void
gst_omx_h265_dec_loop (GstOMXBuffer * buf)
{
  OMX_OTHER_EXTRADATATYPE *pExtraHeader =
      gst_omx_buffer_get_extradata (buf, NVX_ExtraDataVideoDecOutput);

  if (pExtraHeader
      && (pExtraHeader->nDataSize ==
          sizeof (NVX_VIDEO_DEC_OUTPUT_EXTRA_DATA))) {

    NVX_VIDEO_DEC_OUTPUT_EXTRA_DATA *pNvxVideoDecOutputExtraData =
        (NVX_VIDEO_DEC_OUTPUT_EXTRA_DATA *) (&pExtraHeader->data);

    if (pNvxVideoDecOutputExtraData->nDecodeParamsFlag &
        NVX_VIDEO_DEC_OUTPUT_PARAMS_FLAG_FRAME_DPB_REPORT) {

      switch (pNvxVideoDecOutputExtraData->codecData.hevcData.PicType) {
        case OMX_VIDEO_PictureTypeI:
          if (pNvxVideoDecOutputExtraData->codecData.hevcData.sDecDpbReport.
              currentFrame.bIdrFrame) {
            buf->Video_Meta.VideoDecMeta.dec_frame_type = IDR_FRAME;
          } else {
            buf->Video_Meta.VideoDecMeta.dec_frame_type = I_FRAME;
          }
          break;
        case OMX_VIDEO_PictureTypeP:
        {
          buf->Video_Meta.VideoDecMeta.dec_frame_type = P_FRAME;
        }
          break;
        case OMX_VIDEO_PictureTypeB:
        {
          buf->Video_Meta.VideoDecMeta.dec_frame_type = B_FRAME;
        }
          break;
        default:
          g_print ("Decoded Frame is other than I/P/B\n");
          break;
      }
    }
  }
}
