/* GStreamer
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
#include <gst/audio/gstaudiodecoder.h>
#include "gstomxaacdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_aac_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_aac_dec_debug_category

/* prototypes */

static gboolean gst_omx_aac_dec_set_format (GstOMXAudioDec * decoder,
    GstOMXPort * port, GstCaps * caps);

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstOMXAACDec, gst_omx_aac_dec, GST_TYPE_OMX_AUDIO_DEC,
    GST_DEBUG_CATEGORY_INIT (gst_omx_aac_dec_debug_category, "omxaacdec", 0,
        "debug category for omxaacdec element"));

static void
gst_omx_aac_dec_class_init (GstOMXAACDecClass * klass)
{
  GstOMXAudioDecClass *omxdec_class = GST_OMX_AUDIO_DEC_CLASS (klass);

  omxdec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_aac_dec_set_format);

  omxdec_class->cdata.default_sink_template_caps = "audio/mpeg, "
      "mpegversion = (int) {2,4}, "
      "stream-format = (string) {raw, adts, adif}";

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "OpenMax AAC decoder", "Codec/Decoder/Audio", "Decodes AAC audio streams",
      "Jitendra Kumar <jitendrak@nvidia.com>");

  gst_omx_set_default_role (&omxdec_class->cdata, "audio_decoder.eaacplus");
}

static void
gst_omx_aac_dec_init (GstOMXAACDec * omxaacdec)
{
}

static gboolean
gst_omx_aac_dec_set_format (GstOMXAudioDec * decoder, GstOMXPort * port,
    GstCaps * caps)
{
  GstOMXAACDec *self = GST_OMX_AAC_DEC (decoder);
  OMX_AUDIO_PARAM_AACPROFILETYPE aac_profile;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_ERRORTYPE err;

  gst_omx_port_get_port_definition (port, &port_def);
  port_def.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
  err = gst_omx_port_update_port_definition (port, &port_def);

  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to update  port definition of component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  GST_OMX_INIT_STRUCT (&aac_profile);
  aac_profile.nPortIndex = port->index;

  err = gst_omx_component_get_parameter (decoder->dec, OMX_IndexParamAudioAac,
      &aac_profile);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to get AAC parameters from component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  if (caps) {
    GstStructure *s;
    gint mpegversion = 0;
    const gchar *profile_string, *stream_format_string;

    if (gst_caps_is_empty (caps)) {
      GST_ERROR_OBJECT (self, "Empty caps");
      return FALSE;
    }

    s = gst_caps_get_structure (caps, 0);

    if (gst_structure_get_int (s, "mpegversion", &mpegversion)) {
      profile_string =
          gst_structure_get_string (s,
          ((mpegversion == 2) ? "profile" : "base-profile"));

      if (profile_string) {
        if (g_str_equal (profile_string, "main")) {
          aac_profile.eAACProfile = OMX_AUDIO_AACObjectMain;
        } else if (g_str_equal (profile_string, "lc")) {
          aac_profile.eAACProfile = OMX_AUDIO_AACObjectLC;
        } else if (g_str_equal (profile_string, "ssr")) {
          aac_profile.eAACProfile = OMX_AUDIO_AACObjectSSR;
        } else if (g_str_equal (profile_string, "ltp")) {
          aac_profile.eAACProfile = OMX_AUDIO_AACObjectLTP;
        } else {
          GST_ERROR_OBJECT (self, "Unsupported profile '%s'", profile_string);
          return FALSE;
        }
      }
    }

    stream_format_string = gst_structure_get_string (s, "stream-format");
    if (stream_format_string) {
      if (g_str_equal (stream_format_string, "raw")) {
        aac_profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatRAW;
      } else if (g_str_equal (stream_format_string, "adts")) {
        if (mpegversion == 2) {
          aac_profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP2ADTS;
        } else {
          aac_profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
        }
      } else if (g_str_equal (stream_format_string, "loas")) {
        aac_profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4LOAS;
      } else if (g_str_equal (stream_format_string, "latm")) {
        aac_profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4LATM;
      } else if (g_str_equal (stream_format_string, "adif")) {
        aac_profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatADIF;
      } else {
        GST_ERROR_OBJECT (self, "Unsupported stream-format '%s'",
            stream_format_string);
        return FALSE;
      }
    }

  }

  err = gst_omx_component_set_parameter (decoder->dec, OMX_IndexParamAudioAac,
      &aac_profile);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Error setting AAC parameters: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  return TRUE;
}
