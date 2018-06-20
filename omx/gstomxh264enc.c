/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (c) 2013-2017, NVIDIA CORPORATION.  All rights reserved.
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

#include "gstomxh264enc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_h264_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_h264_enc_debug_category

/* prototypes */
static gboolean gst_omx_h264_enc_set_format (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstCaps *gst_omx_h264_enc_get_caps (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstFlowReturn gst_omx_h264_enc_handle_output_frame (GstOMXVideoEnc *
    self, GstOMXPort * port, GstOMXBuffer * buf, GstVideoCodecFrame * frame);
static void gst_omx_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static OMX_ERRORTYPE gst_omx_set_avc_encoder_property (GstOMXVideoEnc * enc);

enum
{
  PROP_0,
  PROP_INSERT_SPS_PPS,
  PROP_NUM_BFRAMES,
  PROP_SLICE_HEADER_SPACING,
  PROP_PROFILE,
  PROP_INSERT_AUD,
  PROP_INSERT_VUI
};

#define DEFAULT_SLICE_HEADER_SPACING 0
#define DEFAULT_PROFILE OMX_VIDEO_AVCProfileBaseline
#define DEFAULT_NUM_B_FRAMES 0
#define MAX_NUM_B_FRAMES 2


#define GST_TYPE_OMX_VID_ENC_PROFILE (gst_omx_videnc_profile_get_type ())
static GType
gst_omx_videnc_profile_get_type (void)
{
  static volatile gsize profile = 0;
  static const GEnumValue profile_type[] = {
    {OMX_VIDEO_AVCProfileBaseline, "GST_OMX_VIDENC_BASELINE_PROFILE",
        "baseline"},
    {OMX_VIDEO_AVCProfileMain, "GST_OMX_VIDENC_MAIN_PROFILE",
        "main"},
    {OMX_VIDEO_AVCProfileHigh, "GST_OMX_VIDENC_HIGH_PROFILE",
        "high"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&profile)) {
    GType tmp =
        g_enum_register_static ("GstOmxVideoEncProfileType", profile_type);
    g_once_init_leave (&profile, tmp);
  }

  return (GType) profile;
}


/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_h264_enc_debug_category, "omxh264enc", 0, \
      "debug category for gst-omx video encoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXH264Enc, gst_omx_h264_enc,
    GST_TYPE_OMX_VIDEO_ENC, DEBUG_INIT);

static void
gst_omx_h264_enc_class_init (GstOMXH264EncClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOMXVideoEncClass *videoenc_class = GST_OMX_VIDEO_ENC_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_h264_enc_set_format);
  videoenc_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_h264_enc_get_caps);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_omx_h264_enc_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_omx_h264_enc_get_property);

  videoenc_class->cdata.default_src_template_caps = "video/x-h264, "
      "width=(int) [ 16, 4096 ], " "height=(int) [ 16, 4096 ], "
      "stream-format=(string) { byte-stream, avc }, " "alignment=(string) au ";
  videoenc_class->handle_output_frame =
      GST_DEBUG_FUNCPTR (gst_omx_h264_enc_handle_output_frame);

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX H.264 Video Encoder",
      "Codec/Encoder/Video",
      "Encode H.264 video streams",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_omx_set_default_role (&videoenc_class->cdata, "video_encoder.avc");

  g_object_class_install_property (gobject_class, PROP_SLICE_HEADER_SPACING,
      g_param_spec_ulong ("slice-header-spacing", "Slice Header Spacing",
          "Slice Header Spacing number of macroblocks/bits in one packet",
          0, G_MAXULONG, DEFAULT_SLICE_HEADER_SPACING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_INSERT_SPS_PPS,
      g_param_spec_boolean ("insert-sps-pps",
          "Insert H.264 SPS, PPS",
          "Insert H.264 SPS, PPS at every IDR frame",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_BFRAMES,
      g_param_spec_uint ("num-B-Frames",
          "B Frames between two reference frames",
          "Number of B Frames between two reference frames (not recommended)",
          0, MAX_NUM_B_FRAMES, DEFAULT_NUM_B_FRAMES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_PROFILE,
      g_param_spec_enum ("profile", "profile",
          "Set profile for encode",
          GST_TYPE_OMX_VID_ENC_PROFILE, DEFAULT_PROFILE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_INSERT_AUD,
      g_param_spec_boolean ("insert-aud",
          "Insert H.264 AUD",
          "Insert H.264 Access Unit Delimiter(AUD)",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INSERT_VUI,
      g_param_spec_boolean ("insert-vui",
          "Insert H.264 VUI",
          "Insert H.264 VUI(Video Usability Information) in SPS",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_omx_h264_enc_init (GstOMXH264Enc * self)
{
  self->insert_sps_pps = FALSE;
  self->insert_aud = FALSE;
  self->insert_vui = FALSE;
  self->nBFrames = 0;
  self->slice_header_spacing = DEFAULT_SLICE_HEADER_SPACING;
  self->profile = DEFAULT_PROFILE;
}

static OMX_ERRORTYPE
ifi_setup (GstOMXVideoEnc * enc)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if (enc->iframeinterval != 0xffffffff) {
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD oIntraPeriod;
    GST_OMX_INIT_STRUCT (&oIntraPeriod);
    oIntraPeriod.nPortIndex = enc->enc_out_port->index;

    err =
        gst_omx_component_get_config (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexConfigVideoAVCIntraPeriod, &oIntraPeriod);

    if (err == OMX_ErrorNone) {
      if (enc->iframeinterval != 0) {
        oIntraPeriod.nIDRPeriod = enc->iframeinterval;
        oIntraPeriod.nPFrames = enc->iframeinterval - 1;
      }

      err =
          gst_omx_component_set_config (GST_OMX_VIDEO_ENC (self)->enc,
          OMX_IndexConfigVideoAVCIntraPeriod, &oIntraPeriod);
    }
  }
  return err;
}

static OMX_ERRORTYPE
gst_omx_h264_enc_set_params (GstOMXVideoEnc * enc)
{
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  NVX_PARAM_VIDENCPROPERTY oEncodeProp;
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);

  if (self->insert_sps_pps) {
    GST_OMX_INIT_STRUCT (&oEncodeProp);
    oEncodeProp.nPortIndex = enc->enc_out_port->index;

    eError = gst_omx_component_get_index (GST_OMX_VIDEO_ENC (self)->enc,
        (gpointer) NVX_INDEX_PARAM_VIDEO_ENCODE_PROPERTY, &eIndex);

    if (eError == OMX_ErrorNone) {
      eError =
          gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
          eIndex, &oEncodeProp);
      if (eError == OMX_ErrorNone) {
        oEncodeProp.bInsertSPSPPSAtIDR = self->insert_sps_pps;
        oEncodeProp.bInsertAUD = self->insert_aud;
        oEncodeProp.bInsertVUI = self->insert_vui;

        eError =
            gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
            eIndex, &oEncodeProp);
      }
    }
  }
  return eError;
}

static gboolean
gst_omx_h264_enc_set_format (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);
  GstCaps *peercaps;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  OMX_VIDEO_CONFIG_NALSIZE OMXNalSize;
  OMX_ERRORTYPE err;
  const gchar *profile_string, *level_string;
  const gchar *out_format = NULL;

  gst_omx_port_get_port_definition (GST_OMX_VIDEO_ENC (self)->enc_out_port,
      &port_def);
  port_def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  err =
      gst_omx_port_update_port_definition (GST_OMX_VIDEO_ENC
      (self)->enc_out_port, &port_def);
  if (err != OMX_ErrorNone)
    return FALSE;

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;

  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param);
  if (err != OMX_ErrorNone) {
    GST_WARNING_OBJECT (self,
        "Setting profile/level not supported by component");
  }

  peercaps = gst_pad_peer_query_caps (GST_VIDEO_ENCODER_SRC_PAD (enc),
      gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SRC_PAD (enc)));
  if (peercaps) {
    GstStructure *s;

    if (gst_caps_is_empty (peercaps)) {
      gst_caps_unref (peercaps);
      GST_ERROR_OBJECT (self, "Empty caps");
      return FALSE;
    }

    s = gst_caps_get_structure (peercaps, 0);

    if (err == OMX_ErrorNone) {
      profile_string = gst_structure_get_string (s, "profile");
      if (profile_string) {
        if (g_str_equal (profile_string, "baseline")) {
          param.eProfile = OMX_VIDEO_AVCProfileBaseline;
        } else if (g_str_equal (profile_string, "main")) {
          param.eProfile = OMX_VIDEO_AVCProfileMain;
        } else if (g_str_equal (profile_string, "extended")) {
          param.eProfile = OMX_VIDEO_AVCProfileExtended;
        } else if (g_str_equal (profile_string, "high")) {
          param.eProfile = OMX_VIDEO_AVCProfileHigh;
        } else if (g_str_equal (profile_string, "high-10")) {
          param.eProfile = OMX_VIDEO_AVCProfileHigh10;
        } else if (g_str_equal (profile_string, "high-4:2:2")) {
          param.eProfile = OMX_VIDEO_AVCProfileHigh422;
        } else if (g_str_equal (profile_string, "high-4:4:4")) {
          param.eProfile = OMX_VIDEO_AVCProfileHigh444;
        } else {
          goto unsupported_profile;
        }
      }
      level_string = gst_structure_get_string (s, "level");
      if (level_string) {
        if (g_str_equal (level_string, "1")) {
          param.eLevel = OMX_VIDEO_AVCLevel1;
        } else if (g_str_equal (level_string, "1b")) {
          param.eLevel = OMX_VIDEO_AVCLevel1b;
        } else if (g_str_equal (level_string, "1.1")) {
          param.eLevel = OMX_VIDEO_AVCLevel11;
        } else if (g_str_equal (level_string, "1.2")) {
          param.eLevel = OMX_VIDEO_AVCLevel12;
        } else if (g_str_equal (level_string, "1.3")) {
          param.eLevel = OMX_VIDEO_AVCLevel13;
        } else if (g_str_equal (level_string, "2")) {
          param.eLevel = OMX_VIDEO_AVCLevel2;
        } else if (g_str_equal (level_string, "2.1")) {
          param.eLevel = OMX_VIDEO_AVCLevel21;
        } else if (g_str_equal (level_string, "2.2")) {
          param.eLevel = OMX_VIDEO_AVCLevel22;
        } else if (g_str_equal (level_string, "3")) {
          param.eLevel = OMX_VIDEO_AVCLevel3;
        } else if (g_str_equal (level_string, "3.1")) {
          param.eLevel = OMX_VIDEO_AVCLevel31;
        } else if (g_str_equal (level_string, "3.2")) {
          param.eLevel = OMX_VIDEO_AVCLevel32;
        } else if (g_str_equal (level_string, "4")) {
          param.eLevel = OMX_VIDEO_AVCLevel4;
        } else if (g_str_equal (level_string, "4.1")) {
          param.eLevel = OMX_VIDEO_AVCLevel41;
        } else if (g_str_equal (level_string, "4.2")) {
          param.eLevel = OMX_VIDEO_AVCLevel42;
        } else if (g_str_equal (level_string, "5")) {
          param.eLevel = OMX_VIDEO_AVCLevel5;
        } else if (g_str_equal (level_string, "5.1")) {
          param.eLevel = OMX_VIDEO_AVCLevel51;
        } else if (g_str_equal (level_string, "5.2")) {
          param.eLevel = OMX_VIDEO_AVCLevel52;
        } else {
          goto unsupported_level;
        }
      }

      err =
          gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
          OMX_IndexParamVideoProfileLevelCurrent, &param);
      if (err == OMX_ErrorUnsupportedIndex) {
        GST_WARNING_OBJECT (self,
            "Setting profile/level not supported by component");
      } else if (err != OMX_ErrorNone) {
        GST_ERROR_OBJECT (self,
            "Error setting profile %u and level %u: %s (0x%08x)",
            (guint) param.eProfile, (guint) param.eLevel,
            gst_omx_error_to_string (err), err);
        return FALSE;
      }
    }

    out_format = gst_structure_get_string (s, "stream-format");
    if (out_format) {
      if (g_str_equal (out_format, "avc")) {
        self->stream_format = H264_AVC;
      } else if (g_str_equal (out_format, "byte-stream")) {
        self->stream_format = H264_BTS;
        self->insert_sps_pps = TRUE;
      } else {
        goto unsupported_out_format;
      }
    }
    gst_caps_unref (peercaps);
  }

  if (self->stream_format == H264_AVC) {
    GST_OMX_INIT_STRUCT (&OMXNalSize);
    err =
        gst_omx_component_get_config (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexConfigVideoNalSize, &OMXNalSize);
    if (err == OMX_ErrorNone) {
      OMXNalSize.nNaluBytes = 4;
      OMXNalSize.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;
      err =
          gst_omx_component_set_config (GST_OMX_VIDEO_ENC (self)->enc,
          OMX_IndexConfigVideoNalSize, &OMXNalSize);
      if (err != OMX_ErrorNone) {
        return FALSE;
      }
    }
  }


  if (self->insert_sps_pps ||
      self->insert_aud ||
      self->insert_vui) {
    err = gst_omx_h264_enc_set_params (enc);
    if (err != OMX_ErrorNone) {
      GST_WARNING_OBJECT (self,
          "Error setting encode property: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }
  }

  err = gst_omx_set_avc_encoder_property (enc);
  if (err != OMX_ErrorNone) {
    GST_WARNING_OBJECT (self,
        "Error setting avc encoder property: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  if (enc->iframeinterval != 0xffffffff) {
    err = ifi_setup (enc);
    if (err != OMX_ErrorNone) {
      GST_WARNING_OBJECT (self,
          "Error setting iframeinterval %u : %s (0x%08x)",
          (guint) enc->iframeinterval, gst_omx_error_to_string (err), err);
      return FALSE;
    }
  }


  return TRUE;

unsupported_profile:
  GST_ERROR_OBJECT (self, "Unsupported profile %s", profile_string);
  gst_caps_unref (peercaps);
  return FALSE;

unsupported_level:
  GST_ERROR_OBJECT (self, "Unsupported level %s", level_string);
  gst_caps_unref (peercaps);
  return FALSE;

unsupported_out_format:
  GST_ERROR_OBJECT (self, "Unsupported stream-format %s", out_format);
  gst_caps_unref (peercaps);
  return FALSE;
}

static GstCaps *
gst_omx_h264_enc_get_caps (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);
  GstCaps *caps;
  OMX_ERRORTYPE err;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  const gchar *profile, *level, *out_format;

  caps = gst_caps_new_simple ("video/x-h264",
      "alignment", G_TYPE_STRING, "au", NULL);

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;

  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param);
  if (err != OMX_ErrorNone && err != OMX_ErrorUnsupportedIndex
      && err != OMX_ErrorNotImplemented)
    return NULL;

  if (err == OMX_ErrorNone) {
    switch (param.eProfile) {
      case OMX_VIDEO_AVCProfileBaseline:
        profile = "baseline";
        break;
      case OMX_VIDEO_AVCProfileMain:
        profile = "main";
        break;
      case OMX_VIDEO_AVCProfileExtended:
        profile = "extended";
        break;
      case OMX_VIDEO_AVCProfileHigh:
        profile = "high";
        break;
      case OMX_VIDEO_AVCProfileHigh10:
        profile = "high-10";
        break;
      case OMX_VIDEO_AVCProfileHigh422:
        profile = "high-4:2:2";
        break;
      case OMX_VIDEO_AVCProfileHigh444:
        profile = "high-4:4:4";
        break;
      default:
        g_assert_not_reached ();
        return NULL;
    }

    switch (param.eLevel) {
      case OMX_VIDEO_AVCLevel1:
        level = "1";
        break;
      case OMX_VIDEO_AVCLevel1b:
        level = "1b";
        break;
      case OMX_VIDEO_AVCLevel11:
        level = "1.1";
        break;
      case OMX_VIDEO_AVCLevel12:
        level = "1.2";
        break;
      case OMX_VIDEO_AVCLevel13:
        level = "1.3";
        break;
      case OMX_VIDEO_AVCLevel2:
        level = "2";
        break;
      case OMX_VIDEO_AVCLevel21:
        level = "2.1";
        break;
      case OMX_VIDEO_AVCLevel22:
        level = "2.2";
        break;
      case OMX_VIDEO_AVCLevel3:
        level = "3";
        break;
      case OMX_VIDEO_AVCLevel31:
        level = "3.1";
        break;
      case OMX_VIDEO_AVCLevel32:
        level = "3.2";
        break;
      case OMX_VIDEO_AVCLevel4:
        level = "4";
        break;
      case OMX_VIDEO_AVCLevel41:
        level = "4.1";
        break;
      case OMX_VIDEO_AVCLevel42:
        level = "4.2";
        break;
      case OMX_VIDEO_AVCLevel5:
        level = "5";
        break;
      case OMX_VIDEO_AVCLevel51:
        level = "5.1";
        break;
      case OMX_VIDEO_AVCLevel52:
        level = "5.2";
        break;
      default:
        g_assert_not_reached ();
        return NULL;
    }

    gst_caps_set_simple (caps,
        "profile", G_TYPE_STRING, profile, "level", G_TYPE_STRING, level, NULL);
  }

  switch (self->stream_format) {
    case H264_AVC:
      out_format = "avc";
      break;
    case H264_BTS:
      out_format = "byte-stream";
      break;
    default:
      g_assert_not_reached ();
      return NULL;
  }

  gst_caps_set_simple (caps, "stream-format", G_TYPE_STRING, out_format, NULL);

  return caps;
}

static GstFlowReturn
gst_omx_h264_enc_handle_output_frame (GstOMXVideoEnc * self, GstOMXPort * port,
    GstOMXBuffer * buf, GstVideoCodecFrame * frame)
{
  GstOMXH264Enc *h264enc = GST_OMX_H264_ENC (self);

  if (buf->omx_buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
    /* The codec data is SPS/PPS with a startcode => bytestream stream format
     * For bytestream stream format the SPS/PPS is only in-stream and not
     * in the caps!
     */
    if (buf->omx_buf->nFilledLen >= 4 &&
        GST_READ_UINT32_BE (buf->omx_buf->pBuffer +
            buf->omx_buf->nOffset) == 0x00000001) {

#ifndef USE_OMX_TARGET_TEGRA
      GList *l = NULL;
      GstBuffer *hdrs;
      GstMapInfo map = GST_MAP_INFO_INIT;

      GST_DEBUG_OBJECT (self, "got codecconfig in byte-stream format");
      buf->omx_buf->nFlags &= ~OMX_BUFFERFLAG_CODECCONFIG;

      hdrs = gst_buffer_new_and_alloc (buf->omx_buf->nFilledLen);

      gst_buffer_map (hdrs, &map, GST_MAP_WRITE);
      if (map.data) {
        memcpy (map.data,
            buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
            buf->omx_buf->nFilledLen);
      }
      gst_buffer_unmap (hdrs, &map);
      l = g_list_append (l, hdrs);
      gst_video_encoder_set_headers (GST_VIDEO_ENCODER (self), l);
#else
      /* No need to send headers in case of byte-stream.
       * Attach SPS and PPS instead */
      gst_video_codec_frame_unref (frame);
      return GST_FLOW_OK;
#endif
    }
#ifdef USE_OMX_TARGET_TEGRA
    else {
      if (h264enc->stream_format == H264_AVC) {
        gsize inbuf_size;
        GstBuffer *codec_data_buf;
        GstMapInfo cdmap = GST_MAP_INFO_INIT;
        OMX_U32 length_to_allocate, offset;
        OMX_U8 *str_ptr;
        OMX_U8 *pts_ptr;
        OMX_U8 data[6];
        OMX_U32 *codec_data;
        OMX_U32 sps_length;

        GST_DEBUG_OBJECT (self, "create codec_data caps buffer");

        str_ptr = (buf->omx_buf->pBuffer + buf->omx_buf->nOffset);
        inbuf_size = buf->omx_buf->nFilledLen;

        codec_data =
            (OMX_U32 *) (buf->omx_buf->pBuffer + buf->omx_buf->nOffset);
        sps_length = (codec_data[0] >> 24);

        // Need additional 7 bytes of data to indicate no of sps,pps
        //and how many bytes indicate nal_length,profile etc also we
        //are going to strip down 2 bytes each for the sps and pps
        //length hence reducing the size by 4

        length_to_allocate = (inbuf_size + ADDITIONAL_LENGTH);

        codec_data_buf = gst_buffer_new_and_alloc (length_to_allocate);
        gst_buffer_map (codec_data_buf, &cdmap, GST_MAP_WRITE);

        data[0] = 0x01;
        switch (h264enc->profile) {
          case OMX_VIDEO_AVCProfileBaseline:
            data[1] = 0x42;     // 66 Baseline profile
            break;
          case OMX_VIDEO_AVCProfileMain:
            data[1] = 0x4D;     // 77 Main profile
            break;
          case OMX_VIDEO_AVCProfileHigh:
            data[1] = 0x64;     // 100 High profile
            break;
        }
        data[2] = 0x40;         // constrained Baseline
        data[3] = 0x15;         // level2.1
        data[4] = 0x03;         // nal length = 4 bytes
        data[5] = 0x01;         // 1SPS

        // append data of 6 bytes and then the sps length and sps data
        if (cdmap.data) {
          memcpy (cdmap.data, data, HEADER_DATA_LENGTH);

          // Strip down the length indicating the NAL data size from 4 bytes to 2 bytes.
          // This is because the down stream qtmux are expecting this to be only 2 bytes

          memcpy ((cdmap.data + HEADER_DATA_LENGTH), (str_ptr + 2),
              DOWNSTREAM_NAL_LENGTH_INDICATOR);

          // Now copy the SPS data
          memcpy ((cdmap.data + HEADER_DATA_LENGTH + 2),
              (str_ptr + ENCODER_NAL_LENGTH_INDICATOR), sps_length);

          offset =
              HEADER_DATA_LENGTH + DOWNSTREAM_NAL_LENGTH_INDICATOR + sps_length;

          pts_ptr = cdmap.data + offset;
          pts_ptr[0] = 1;         // 1 pps

          offset += 1;

          // Strip down the length indicating the NAL data size from 4 bytes to 2 bytes.
          // This is because the down stream qtmux are expecting this to be only 2 bytes
          memcpy ((cdmap.data + offset),
              (str_ptr + (ENCODER_NAL_LENGTH_INDICATOR + sps_length) + 2),
              DOWNSTREAM_NAL_LENGTH_INDICATOR);

          offset += 2;

          // Now copy pps data
          memcpy ((cdmap.data + offset),
              (str_ptr + ((ENCODER_NAL_LENGTH_INDICATOR << 1) + sps_length)),
              (inbuf_size - ((ENCODER_NAL_LENGTH_INDICATOR << 1) + sps_length)));

          gst_buffer_unmap (codec_data_buf, &cdmap);

          self->codec_data = codec_data_buf;
        }
      }
    }
#endif
  }

  return
      GST_OMX_VIDEO_ENC_CLASS
      (gst_omx_h264_enc_parent_class)->handle_output_frame (self, port, buf,
      frame);
}

static void
gst_omx_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (object);

  switch (prop_id) {
    case PROP_INSERT_SPS_PPS:
      self->insert_sps_pps = g_value_get_boolean (value);
      break;
    case PROP_INSERT_AUD:
      self->insert_aud = g_value_get_boolean (value);
      break;
    case PROP_INSERT_VUI:
      self->insert_vui = g_value_get_boolean (value);
      break;
    case PROP_NUM_BFRAMES:
      self->nBFrames = g_value_get_uint (value);
      break;
    case PROP_SLICE_HEADER_SPACING:
      self->slice_header_spacing = g_value_get_ulong (value);
      break;
    case PROP_PROFILE:
      self->profile = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_h264_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (object);

  switch (prop_id) {
    case PROP_INSERT_SPS_PPS:
      g_value_set_boolean (value, self->insert_sps_pps);
      break;
    case PROP_INSERT_AUD:
      g_value_set_boolean (value, self->insert_aud);
      break;
    case PROP_INSERT_VUI:
      g_value_set_boolean (value, self->insert_vui);
      break;
    case PROP_NUM_BFRAMES:
      g_value_set_uint (value, self->nBFrames);
      break;
    case PROP_SLICE_HEADER_SPACING:
      g_value_set_ulong (value, self->slice_header_spacing);
      break;
    case PROP_PROFILE:
      g_value_set_enum (value, self->profile);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static OMX_ERRORTYPE
gst_omx_set_avc_encoder_property (GstOMXVideoEnc * enc)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);

  OMX_VIDEO_PARAM_AVCTYPE oH264Type;

  GST_OMX_INIT_STRUCT (&oH264Type);
  oH264Type.nPortIndex = 0;

  eError =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoAvc, &oH264Type);
  if (eError == OMX_ErrorNone) {
    oH264Type.nBFrames = self->nBFrames;
    oH264Type.nSliceHeaderSpacing = self->slice_header_spacing;
    oH264Type.eProfile = self->profile;

    eError =
        gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexParamVideoAvc, &oH264Type);
  }
  return eError;
}
