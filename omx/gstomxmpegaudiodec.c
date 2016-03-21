/* GStreamer
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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
#include "gstomxmpegaudiodec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_mpegaudio_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_mpegaudio_dec_debug_category

/* prototypes */

static gboolean gst_omx_mpegaudio_dec_set_format (GstOMXAudioDec * decoder,
    GstOMXPort * port, GstCaps * caps);

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstOMXMPEGAUDIODec, gst_omx_mpegaudio_dec,
    GST_TYPE_OMX_AUDIO_DEC,
    GST_DEBUG_CATEGORY_INIT (gst_omx_mpegaudio_dec_debug_category,
        "omxmpegaudiodec", 0, "debug category for omxmpegaudiodec element"));

static void
gst_omx_mpegaudio_dec_class_init (GstOMXMPEGAUDIODecClass * klass)
{
  GstOMXAudioDecClass *omxdec_class = GST_OMX_AUDIO_DEC_CLASS (klass);

  omxdec_class->set_format =
      GST_DEBUG_FUNCPTR (gst_omx_mpegaudio_dec_set_format);

  omxdec_class->cdata.default_sink_template_caps = "audio/mpeg, "
      "mpegversion = (int) 1, " "layer = (int) [1, 3]";

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "OpenMax MP3/MP2 decoder", "Codec/Decoder/Audio",
      "Decodes MP3 and MP2 audio streams",
      "Jitendra Kumar <jitendrak@nvidia.com>");

  gst_omx_set_default_role (&omxdec_class->cdata, "audio_decoder.mp3");
}

static void
gst_omx_mpegaudio_dec_init (GstOMXMPEGAUDIODec * omxmpegaudiodec)
{
}

static gboolean
gst_omx_mpegaudio_dec_set_format (GstOMXAudioDec * decoder, GstOMXPort * port,
    GstCaps * caps)
{
  GstOMXMPEGAUDIODec *self = GST_OMX_MPEGAUDIO_DEC (decoder);
  OMX_AUDIO_PARAM_MP3TYPE mp3_type;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_ERRORTYPE err;

  gst_omx_port_get_port_definition (port, &port_def);
  port_def.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
  err = gst_omx_port_update_port_definition (port, &port_def);

  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to update  port definition of component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  GST_OMX_INIT_STRUCT (&mp3_type);
  mp3_type.nPortIndex = port->index;

  err = gst_omx_component_get_parameter (decoder->dec, OMX_IndexParamAudioMp3,
      &mp3_type);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to get MP3/MP2 parameters from component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  if (caps) {
    GstStructure *s;
    gint mpegaudioversion = 0, channels = 0, sample_rate = 0, layer = 0;

    if (gst_caps_is_empty (caps)) {
      GST_ERROR_OBJECT (self, "Empty caps");
      return FALSE;
    }

    s = gst_caps_get_structure (caps, 0);

    if (gst_structure_get_int (s, "mpegaudioversion", &mpegaudioversion)) {

      if (gst_structure_get_int (s, "layer", &layer) && layer == 3) {
        if (mpegaudioversion == 1)
          mp3_type.eFormat = OMX_AUDIO_MP3StreamFormatMP1Layer3;
        else if (mpegaudioversion == 2)
          mp3_type.eFormat = OMX_AUDIO_MP3StreamFormatMP2Layer3;
      }
    }
    if (gst_structure_get_int (s, "channels", &channels))
      mp3_type.nChannels = channels;

    if (gst_structure_get_int (s, "rate", &sample_rate))
      mp3_type.nSampleRate = sample_rate;
  }

  err = gst_omx_component_set_parameter (decoder->dec, OMX_IndexParamAudioMp3,
      &mp3_type);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Error setting MP3/MP2 parameters: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }
  return TRUE;
}
