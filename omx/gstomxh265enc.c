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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <gst/gst.h>

#include "gstomxh265enc.h"
#include "OMX_IndexExt.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_h265_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_h265_enc_debug_category

/* prototypes */
static gboolean gst_omx_h265_enc_set_format (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstCaps *gst_omx_h265_enc_get_caps (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstFlowReturn gst_omx_h265_enc_handle_output_frame (GstOMXVideoEnc *
    self, GstOMXPort * port, GstOMXBuffer * buf, GstVideoCodecFrame * frame);

static void gst_omx_h265_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_h265_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static OMX_ERRORTYPE gst_omx_h265_enc_set_params (GstOMXVideoEnc * enc);

enum
{
  PROP_0,
  PROP_SLICE_HEADER_SPACING,
  PROP_INSERT_AUD,
  PROP_INSERT_VUI
};

#ifdef USE_OMX_TARGET_TEGRA
#define SINK_NVFORMATS "I420, I420_10LE, NV12"
#define SINK_FORMATS "I420, NV12"
#endif
#define DEFAULT_SLICE_HEADER_SPACING 0

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_h265_enc_debug_category, "omxh265enc", 0, \
      "debug category for gst-omx video encoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXH265Enc, gst_omx_h265_enc,
    GST_TYPE_OMX_VIDEO_ENC, DEBUG_INIT);

static void
gst_omx_h265_enc_class_init (GstOMXH265EncClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOMXVideoEncClass *videoenc_class = GST_OMX_VIDEO_ENC_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_h265_enc_set_format);
  videoenc_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_h265_enc_get_caps);
  videoenc_class->handle_output_frame =
      GST_DEBUG_FUNCPTR (gst_omx_h265_enc_handle_output_frame);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_omx_h265_enc_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_omx_h265_enc_get_property);

  videoenc_class->cdata.default_sink_template_caps = "video/x-raw(memory:NVMM), "
#ifdef USE_OMX_TARGET_TEGRA
      "format = (string) { " SINK_NVFORMATS " }, "
#endif
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", " "framerate = " GST_VIDEO_FPS_RANGE
      ";" "video/x-raw, "
#ifdef USE_OMX_TARGET_TEGRA
      "format = (string) { " SINK_FORMATS " }, "
#endif
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", " "framerate = " GST_VIDEO_FPS_RANGE;

  videoenc_class->cdata.default_src_template_caps = "video/x-h265, "
      "width=(int) [ 1, MAX ], " "height=(int) [ 1, MAX ], "
      "framerate = (fraction) [0, MAX], "
      "stream-format=(string) { byte-stream, hvc1 }, "
      "alignment=(string) au ";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX H.265 Video Encoder",
      "Codec/Encoder/Video",
      "Encode H.265 video streams", "Sanket Kothari <skothari@nvidia.com>");

  gst_omx_set_default_role (&videoenc_class->cdata, "video_encoder.hevc");

  g_object_class_install_property (gobject_class, PROP_SLICE_HEADER_SPACING,
      g_param_spec_ulong ("slice-header-spacing", "Slice Header Spacing",
          "Slice Header Spacing number of macroblocks/bits in one packet",
          0, G_MAXULONG, DEFAULT_SLICE_HEADER_SPACING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_INSERT_AUD,
      g_param_spec_boolean ("insert-aud",
          "Insert H.265 AUD",
          "Insert H.265 Access Unit Delimiter(AUD)",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INSERT_VUI,
      g_param_spec_boolean ("insert-vui",
          "Insert H.265 VUI",
          "Insert H.265 VUI(Video Usability Information) in SPS",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_omx_h265_enc_init (GstOMXH265Enc * self)
{
  self->insert_sps_pps = FALSE;
  self->insert_aud = FALSE;
  self->insert_vui = FALSE;
  self->slice_header_spacing = DEFAULT_SLICE_HEADER_SPACING;
}

static gboolean
gst_omx_h265_enc_set_format (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXH265Enc *self = GST_OMX_H265_ENC (enc);
  GstCaps *peercaps;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  OMX_ERRORTYPE err;
  const gchar *profile_string = NULL, *level_string = NULL;
  const gchar *out_format = NULL;

  gst_omx_port_get_port_definition (GST_OMX_VIDEO_ENC (self)->enc_out_port,
      &port_def);
  port_def.format.video.eCompressionFormat = NVX_VIDEO_CodingHEVC;
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
        if (g_str_equal (profile_string, "main")) {
          param.eProfile = OMX_VIDEO_HEVCProfileMain;
        } else if (g_str_equal (profile_string, "main-10")) {
          param.eProfile = OMX_VIDEO_HEVCProfileMain10;
        } else if (g_str_equal (profile_string, "mainstillpicture")) {
          param.eProfile = OMX_VIDEO_HEVCProfileMainStillPicture;
        } else {
          goto unsupported_profile;
        }
      }

      level_string = gst_structure_get_string (s, "level");
      if (level_string) {
        if (g_str_equal (level_string, "main1")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel1;
        } else if (g_str_equal (level_string, "main2")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel2;
        } else if (g_str_equal (level_string, "main2.1")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel21;
        } else if (g_str_equal (level_string, "main3")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel3;
        } else if (g_str_equal (level_string, "main3.1")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel31;
        } else if (g_str_equal (level_string, "main4")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel4;
        } else if (g_str_equal (level_string, "main4.1")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel41;
        } else if (g_str_equal (level_string, "main5")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel5;
        } else if (g_str_equal (level_string, "main5.1")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel51;
        } else if (g_str_equal (level_string, "main5.2")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel52;
        } else if (g_str_equal (level_string, "main6")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel6;
        } else if (g_str_equal (level_string, "main6.1")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel61;
        } else if (g_str_equal (level_string, "main6.2")) {
          param.eLevel = OMX_VIDEO_HEVCMainTierLevel62;
        } else if (g_str_equal (level_string, "high1")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel1;
        } else if (g_str_equal (level_string, "high2")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel2;
        } else if (g_str_equal (level_string, "high2.1")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel21;
        } else if (g_str_equal (level_string, "high3")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel3;
        } else if (g_str_equal (level_string, "high3.1")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel31;
        } else if (g_str_equal (level_string, "high4")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel4;
        } else if (g_str_equal (level_string, "high4.1")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel41;
        } else if (g_str_equal (level_string, "high5")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel5;
        } else if (g_str_equal (level_string, "high5.1")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel51;
        } else if (g_str_equal (level_string, "high5.2")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel52;
        } else if (g_str_equal (level_string, "high6")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel6;
        } else if (g_str_equal (level_string, "high6.1")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel61;
        } else if (g_str_equal (level_string, "high6.2")) {
          param.eLevel = OMX_VIDEO_HEVCHighTierLevel62;
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
      if (g_str_equal (out_format, "hvc1")) {
        self->stream_format = H265_HVC1;
      } else if (g_str_equal (out_format, "byte-stream")) {
        self->stream_format = H265_BTS;
        self->insert_sps_pps = TRUE;
      } else {
        goto unsupported_out_format;
      }
    }
    gst_caps_unref (peercaps);
  }

  if (self->stream_format == H265_HVC1) {
    OMX_VIDEO_CONFIG_NALSIZE OMXNalSize;
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
    } else {
      return FALSE;
    }
  }

  err = gst_omx_h265_enc_set_params (enc);
  if (err != OMX_ErrorNone) {
      GST_WARNING_OBJECT (self,
              "Error setting encoder params: %s (0x%08x)",
              gst_omx_error_to_string (err), err);
      return FALSE;
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
gst_omx_h265_enc_get_caps (GstOMXVideoEnc * enc, GstOMXPort * port,
        GstVideoCodecState * state)
{
    GstOMXH265Enc *self = GST_OMX_H265_ENC (enc);
    GstCaps *caps;
    OMX_ERRORTYPE err;
    OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
    const gchar *profile, *level, *out_format;

    caps = gst_caps_new_simple ("video/x-h265",
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
            case OMX_VIDEO_HEVCProfileMain:
                profile = "main";
                break;
            case OMX_VIDEO_HEVCProfileMain10:
                profile = "main-10";
                break;
            case OMX_VIDEO_HEVCProfileMainStillPicture:
                profile = "mainstillpicture";
                break;

            default:
                g_assert_not_reached ();
                return NULL;
        }

        switch (param.eLevel) {
            case OMX_VIDEO_HEVCMainTierLevel1:
                level = "main1";
                break;
            case OMX_VIDEO_HEVCMainTierLevel2:
                level = "main2";
                break;
            case OMX_VIDEO_HEVCMainTierLevel21:
                level = "main2.1";
                break;
            case OMX_VIDEO_HEVCMainTierLevel3:
                level = "main3";
                break;
            case OMX_VIDEO_HEVCMainTierLevel31:
                level = "main3.1";
                break;
            case OMX_VIDEO_HEVCMainTierLevel4:
                level = "main4";
                break;
            case OMX_VIDEO_HEVCMainTierLevel41:
                level = "main4.1";
                break;
            case OMX_VIDEO_HEVCMainTierLevel5:
                level = "main5";
                break;
            case OMX_VIDEO_HEVCMainTierLevel51:
                level = "main5.1";
                break;
            case OMX_VIDEO_HEVCMainTierLevel52:
                level = "main5.2";
                break;
            case OMX_VIDEO_HEVCMainTierLevel6:
                level = "main6";
                break;
            case OMX_VIDEO_HEVCMainTierLevel61:
                level = "main6.1";
                break;
      case OMX_VIDEO_HEVCMainTierLevel62:
        level = "main6.2";
        break;
      case OMX_VIDEO_HEVCHighTierLevel1:
        level = "high1";
        break;
      case OMX_VIDEO_HEVCHighTierLevel2:
        level = "high2";
        break;
      case OMX_VIDEO_HEVCHighTierLevel21:
        level = "high2.1";
        break;
      case OMX_VIDEO_HEVCHighTierLevel3:
        level = "high3";
        break;
      case OMX_VIDEO_HEVCHighTierLevel31:
        level = "high3.1";
        break;
      case OMX_VIDEO_HEVCHighTierLevel4:
        level = "high4";
        break;
      case OMX_VIDEO_HEVCHighTierLevel41:
        level = "high4.1";
        break;
      case OMX_VIDEO_HEVCHighTierLevel5:
        level = "high5";
        break;
      case OMX_VIDEO_HEVCHighTierLevel51:
        level = "high5.1";
        break;
      case OMX_VIDEO_HEVCHighTierLevel52:
        level = "high5.2";
        break;
      case OMX_VIDEO_HEVCHighTierLevel6:
        level = "high6";
        break;
      case OMX_VIDEO_HEVCHighTierLevel61:
        level = "high6.1";
        break;
      case OMX_VIDEO_HEVCHighTierLevel62:
        level = "high6.2";
        break;

      default:
        g_assert_not_reached ();
        return NULL;
    }

    gst_caps_set_simple (caps,
        "profile", G_TYPE_STRING, profile, "level", G_TYPE_STRING, level, NULL);
  }

  switch (self->stream_format) {
    case H265_HVC1:
      out_format = "hvc1";
      break;
    case H265_BTS:
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
gst_omx_h265_enc_handle_output_frame (GstOMXVideoEnc * self, GstOMXPort * port,
    GstOMXBuffer * buf, GstVideoCodecFrame * frame)
{
  GstOMXH265Enc *h265enc = GST_OMX_H265_ENC (self);

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
    else if(h265enc->stream_format == H265_HVC1) {
      OMX_U8 *str_ptr, *sps_ptr;
      OMX_U32 vps_size, sps_size, pps_size;
      GstBuffer *codec_data_buf;
      GstMapInfo map = GST_MAP_INFO_INIT;
      guint8 max_sub_layers_minus1, temporal_id_nesting_flag;
      guint8 *data, i, zero_byte;
      gint nl = 4; // nal_length_size

      // get size
      str_ptr = (buf->omx_buf->pBuffer + buf->omx_buf->nOffset);

      vps_size = *(str_ptr + (nl - 1));
      str_ptr += (nl + vps_size);

      sps_size = *(str_ptr + (nl - 1));
      sps_ptr = str_ptr + nl;
      max_sub_layers_minus1 = (sps_ptr[2] & 0x7) >> 1;
      temporal_id_nesting_flag = sps_ptr[2] & 0x1;
      str_ptr += (nl + sps_size);

      pps_size = *(str_ptr + (nl - 1));
      str_ptr += (nl + pps_size);

      // put codec data
      codec_data_buf =
        gst_buffer_new_allocate (NULL,
        23 + 18 + vps_size + sps_size + pps_size,
        NULL);
      gst_buffer_map (codec_data_buf, &map, GST_MAP_WRITE);
      data = map.data;
      memset (data, 0, map.size);

      data[0] = 1; /* HEVCDecoderConfigurationVersion = 1 */

      sps_ptr += 3;
      i = 1;
      zero_byte = 0;
      while (i < 13) {
        if (zero_byte == 2 && *sps_ptr == 3) {
          // discard the padding 0x03
          zero_byte = 0;
        } else {
          data[i] = *sps_ptr;
          i++;
          if (*sps_ptr == 0)
            zero_byte++;
          else
            zero_byte = 0;
        }
        sps_ptr++;
      }

      data[13] = 0xf0;
      data[15] = 0xfc;
      data[16] = 0xfd; /* chroma_format_idc = 1 */
      data[17] = 0xf8; /* bit_depth_luma_minus8 = 0 */
      data[18] = 0xf8; /* bit_depth_chroma_minus8 = 0 */
      data[19] = 0x00; /* keep avgFrameRate as unspecified */
      data[20] = 0x00; /* keep avgFrameRate as unspecified */
      /* constFrameRate(2 bits): 0, stream may or may not be of constant framerate
       * numTemporalLayers (3 bits): number of temporal layers, value from SPS
       * TemporalIdNested (1 bit): sps_temporal_id_nesting_flag from SPS
       * lengthSizeMinusOne (2 bits): plus 1 indicates the length of the NALUnitLength */
      data[21] = 0x00 |
        ((max_sub_layers_minus1 + 1) << 3) |
        (temporal_id_nesting_flag << 2) | (nl - 1);
      GST_WRITE_UINT8 (data + 22, 3); /* num_arrays = 3 */
      data += 23;

      str_ptr = (buf->omx_buf->pBuffer + buf->omx_buf->nOffset);
      /* vps array */
      data[0] = 0x20;
      data ++;
      GST_WRITE_UINT16_BE (data, 1);
      data += 2;
      GST_WRITE_UINT16_BE (data, vps_size);
      data += 2;
      memcpy(data, str_ptr + nl, vps_size);
      data += vps_size;
      str_ptr += (nl + vps_size);
      /* sps array */
      data[0] = 0x21;
      data ++;
      GST_WRITE_UINT16_BE (data, 1);
      data += 2;
      GST_WRITE_UINT16_BE (data, sps_size);
      data += 2;
      memcpy(data, str_ptr + nl, sps_size);
      data += sps_size;
      str_ptr += (nl + sps_size);
      /* pps array */
      data[0] = 0x22;
      data ++;
      GST_WRITE_UINT16_BE (data, 1);
      data += 2;
      GST_WRITE_UINT16_BE (data, pps_size);
      data += 2;
      memcpy(data, str_ptr + nl, pps_size);

      gst_buffer_unmap (codec_data_buf, &map);
      self->codec_data = codec_data_buf;
    }
#endif
  }

  return
      GST_OMX_VIDEO_ENC_CLASS
      (gst_omx_h265_enc_parent_class)->handle_output_frame (self, port, buf,
      frame);
}


static void
gst_omx_h265_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOMXH265Enc *self = GST_OMX_H265_ENC (object);

  switch (prop_id) {
    case PROP_SLICE_HEADER_SPACING:
      self->slice_header_spacing = g_value_get_ulong (value);
      break;
    case PROP_INSERT_AUD:
      self->insert_aud = g_value_get_boolean (value);
      break;
    case PROP_INSERT_VUI:
      self->insert_vui = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_h265_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOMXH265Enc *self = GST_OMX_H265_ENC (object);

  switch (prop_id) {
    case PROP_SLICE_HEADER_SPACING:
      g_value_set_ulong (value, self->slice_header_spacing);
      break;
    case PROP_INSERT_AUD:
      g_value_set_boolean (value, self->insert_aud);
      break;
    case PROP_INSERT_VUI:
      g_value_set_boolean (value, self->insert_vui);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static OMX_ERRORTYPE
gst_omx_h265_enc_set_params (GstOMXVideoEnc * enc)
{
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  NVX_PARAM_VIDENCPROPERTY oEncodeProp;
  GstOMXH265Enc *self = GST_OMX_H265_ENC (enc);

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
          oEncodeProp.codecParams.hevc.nSliceHeaderSpacing = self->slice_header_spacing;
          oEncodeProp.bInsertAUD = self->insert_aud;
          oEncodeProp.bInsertVUI = self->insert_vui;

          eError =
              gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
                      eIndex, &oEncodeProp);
      }
  }

  if (enc->iframeinterval != 0xffffffff) {
    OMX_VIDEO_PARAM_HEVCTYPE oH265Type;

    GST_OMX_INIT_STRUCT (&oH265Type);
    oH265Type.nPortIndex = 0;

    eError =
        gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexParamVideoHevc, &oH265Type);
    if (eError == OMX_ErrorNone) {
      if (enc->iframeinterval != 0)
        oH265Type.nPFrames = enc->iframeinterval - 1;
      eError =
          gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
          OMX_IndexParamVideoHevc, &oH265Type);
    }
  }

  return eError;
}
