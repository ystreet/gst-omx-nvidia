/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "gstomxh264dec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_h264_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_h264_dec_debug_category

/* prototypes */
static gboolean gst_omx_h264_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);
static gboolean gst_omx_h264_dec_set_format (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);
static void gst_omx_h264_dec_loop (GstOMXBuffer * buf);
static GstStateChangeReturn
gst_omx_h264_dec_change_state (GstElement * element,
    GstStateChange transition);

enum
{
  PROP_0,
  PROP_ENABLE_FRAME_TYPE_REPORTING
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_h264_dec_debug_category, "omxh264dec", 0, \
      "debug category for gst-omx video decoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXH264Dec, gst_omx_h264_dec,
    GST_TYPE_OMX_VIDEO_DEC, DEBUG_INIT);

static void
gst_omx_h264_dec_set_property (GObject * object,
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
gst_omx_h264_dec_get_property (GObject * object,
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
gst_omx_h264_dec_class_init (GstOMXH264DecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstOMXVideoDecClass *videodec_class = GST_OMX_VIDEO_DEC_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  videodec_class->is_format_change =
      GST_DEBUG_FUNCPTR (gst_omx_h264_dec_is_format_change);
  videodec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_h264_dec_set_format);
  videodec_class->video_dec_loop = GST_DEBUG_FUNCPTR (gst_omx_h264_dec_loop);
  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_h264_dec_change_state);

  gobject_class->set_property = gst_omx_h264_dec_set_property;
  gobject_class->get_property = gst_omx_h264_dec_get_property;

  videodec_class->cdata.default_sink_template_caps = "video/x-h264, "
      "parsed=(boolean) true, "
      "alignment=(string) au, "
      "stream-format=(string) byte-stream, "
      "width=(int) [1,MAX], " "height=(int) [1,MAX]";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX H.264 Video Decoder",
      "Codec/Decoder/Video",
      "Decode H.264 video streams",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  g_object_class_install_property (gobject_class, PROP_ENABLE_FRAME_TYPE_REPORTING,
      g_param_spec_boolean ("enable-frame-type-reporting",
          "enable-frame-type-reporting",
          "Set to enable frame type reporting",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_omx_set_default_role (&videodec_class->cdata, "video_decoder.avc");
}

static void
gst_omx_h264_dec_init (GstOMXH264Dec * self)
{
  GstOMXVideoDec *dec = GST_OMX_VIDEO_DEC (self);

#ifdef USE_OMX_TARGET_TEGRA
  dec->disable_dpb = FALSE;
  dec->skip_frames = GST_DECODE_ALL;
#endif
}

static gboolean
gst_omx_h264_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state)
{
  return FALSE;
}

static OMX_ERRORTYPE
gstomx_set_disable_dpb_property (OMX_HANDLETYPE omx_handle)
{
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  NVX_PARAM_H264DISABLE_DPB disable_dpb;
  GST_OMX_INIT_STRUCT (&disable_dpb);
  eError =
      OMX_GetExtensionIndex (omx_handle,
      (OMX_STRING) "OMX.Nvidia.index.param.h264disabledpb", &eIndex);
  if (eError == OMX_ErrorNone) {
    disable_dpb.bDisableDPB = OMX_TRUE;
    OMX_SetParameter (omx_handle, eIndex, &disable_dpb);
  }
  return eError;
}

static OMX_ERRORTYPE
gst_omx_h264_dec_set_skip_frame (OMX_HANDLETYPE omx_handle,
    GstVideoSkipFrames skip_frames)
{
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_CONFIG_BOOLEANTYPE omx_config_skip_frames;
  GST_OMX_INIT_STRUCT (&omx_config_skip_frames);

  switch (skip_frames) {
    case GST_DECODE_ALL:
    {
      /* Do nothing. Default case */
    }
      break;
    case GST_SKIP_NON_REF_FRAMES:
    {
      /* Skip non-ref frames */
      eError =
          OMX_GetExtensionIndex (omx_handle,
          (OMX_STRING) NVX_INDEX_SKIP_NONREF_FRAMES, &eIndex);

      if (eError == OMX_ErrorNone) {
        omx_config_skip_frames.bEnabled = OMX_TRUE;
        eError = OMX_SetConfig (omx_handle, eIndex, &omx_config_skip_frames);
      }
    }
      break;
    case GST_DECODE_KEY_FRAMES:
    {
      /* Decode only key frames */
      eError =
          OMX_GetExtensionIndex (omx_handle,
          (OMX_STRING) NVX_INDEX_CONFIG_DECODE_IFRAMES, &eIndex);

      if (eError == OMX_ErrorNone) {
        omx_config_skip_frames.bEnabled = OMX_TRUE;
        eError = OMX_SetConfig (omx_handle, eIndex, &omx_config_skip_frames);
      }
    }
      break;
  }

  return eError;
}

static gboolean
gst_omx_h264_dec_set_format (GstOMXVideoDec * dec, GstOMXPort * port,
    GstVideoCodecState * state)
{
  gboolean ret;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;

  gst_omx_port_get_port_definition (port, &port_def);
  port_def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

  if (dec->full_frame_data == TRUE) {
    OMX_HANDLETYPE omx_handle = dec->dec->handle;
    gst_omx_set_full_frame_data_property (omx_handle);
  }

  if (dec->disable_dpb == TRUE) {
    OMX_HANDLETYPE omx_handle = dec->dec->handle;
    gstomx_set_disable_dpb_property (omx_handle);
  }

  ret = gst_omx_port_update_port_definition (port, &port_def) == OMX_ErrorNone;

  return ret;
}

static GstStateChangeReturn
gst_omx_h264_dec_change_state (GstElement * element,
    GstStateChange transition)
{
  GstOMXVideoDec * dec = GST_OMX_VIDEO_DEC (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (dec->skip_frames != GST_DECODE_ALL) {
        OMX_HANDLETYPE omx_handle = dec->dec->handle;
        gst_omx_h264_dec_set_skip_frame (omx_handle, dec->skip_frames);
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_omx_h264_dec_parent_class)->change_state
      (element, transition);

  return ret;
}

static void
gst_omx_h264_dec_loop (GstOMXBuffer * buf)
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

      switch (pNvxVideoDecOutputExtraData->codecData.h264Data.PicType) {
        case OMX_VIDEO_PictureTypeI:
          if (pNvxVideoDecOutputExtraData->codecData.h264Data.sDecDpbReport.
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
