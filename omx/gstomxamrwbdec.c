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
#include "gstomxamrwbdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_amrwb_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_amrwb_dec_debug_category

/* prototypes */

static gboolean gst_omx_amrwb_dec_set_format (GstOMXAudioDec * decoder,
    GstOMXPort * port, GstCaps * caps);

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstOMXAMRWBDec, gst_omx_amrwb_dec,
    GST_TYPE_OMX_AUDIO_DEC,
    GST_DEBUG_CATEGORY_INIT (gst_omx_amrwb_dec_debug_category, "omxamrwbdec", 0,
        "debug category for omxamrwbdec element"));

static void
gst_omx_amrwb_dec_class_init (GstOMXAMRWBDecClass * klass)
{
  GstOMXAudioDecClass *omxdec_class = GST_OMX_AUDIO_DEC_CLASS (klass);

  omxdec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_amrwb_dec_set_format);

  omxdec_class->cdata.default_sink_template_caps = "audio/AMR-WB";

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "OpenMax AMR-WB decoder", "Codec/Decoder/Audio",
      "Decodes AMR-WB audio streams", "Jitendra Kumar <jitendrak@nvidia.com>");

  gst_omx_set_default_role (&omxdec_class->cdata, "audio_decoder.amrwb");
}

static void
gst_omx_amrwb_dec_init (GstOMXAMRWBDec * omxamrwbdec)
{
}

static gboolean
gst_omx_amrwb_dec_set_format (GstOMXAudioDec * decoder, GstOMXPort * port,
    GstCaps * caps)
{
  GstOMXAMRWBDec *self = GST_OMX_AMRWB_DEC (decoder);
  OMX_AUDIO_PARAM_AMRTYPE amr_type;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_ERRORTYPE err;

  gst_omx_port_get_port_definition (port, &port_def);
  port_def.format.audio.eEncoding = OMX_AUDIO_CodingAMR;
  err = gst_omx_port_update_port_definition (port, &port_def);

  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to update  port definition of component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  GST_OMX_INIT_STRUCT (&amr_type);
  amr_type.nPortIndex = port->index;

  err = gst_omx_component_get_parameter (decoder->dec, OMX_IndexParamAudioAmr,
      &amr_type);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to get AMR parameters from component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  if (caps) {
    GstStructure *s;
    gint channels = 0;

    if (gst_caps_is_empty (caps)) {
      GST_ERROR_OBJECT (self, "Empty caps");
      return FALSE;
    }

    s = gst_caps_get_structure (caps, 0);

    if (gst_structure_get_int (s, "channels", &channels))
      amr_type.nChannels = channels;
  }

  err = gst_omx_component_set_parameter (decoder->dec, OMX_IndexParamAudioAmr,
      &amr_type);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Error setting AMR parameters: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  return TRUE;
}
