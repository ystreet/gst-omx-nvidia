/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "gstomxwmvdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_wmv_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_wmv_dec_debug_category

/* prototypes */
static gboolean gst_omx_wmv_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);
static gboolean gst_omx_wmv_dec_set_format (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);

#ifdef USE_OMX_TARGET_TEGRA
static GstFlowReturn gst_omx_wmv_dec_prepare_frame (GstOMXVideoDec * self,
    GstVideoCodecFrame * frame);
#endif

enum
{
  PROP_0
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_wmv_dec_debug_category, "omxwmvdec", 0, \
      "debug category for gst-omx video decoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXWMVDec, gst_omx_wmv_dec,
    GST_TYPE_OMX_VIDEO_DEC, DEBUG_INIT);


static void
gst_omx_wmv_dec_class_init (GstOMXWMVDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstOMXVideoDecClass *videodec_class = GST_OMX_VIDEO_DEC_CLASS (klass);

  videodec_class->is_format_change =
      GST_DEBUG_FUNCPTR (gst_omx_wmv_dec_is_format_change);
  videodec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_wmv_dec_set_format);
#ifdef USE_OMX_TARGET_TEGRA
  videodec_class->prepare_frame =
      GST_DEBUG_FUNCPTR (gst_omx_wmv_dec_prepare_frame);
#endif

  videodec_class->cdata.default_sink_template_caps = "video/x-wmv, "
      "wmvversion= (int) 3, "
      "format = (string) {WMV3, WVC1}, "
      "width=(int) [1,MAX], " "height=(int) [1,MAX]";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX WMV Video Decoder",
      "Codec/Decoder/Video",
      "Decode WMV video streams",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_omx_set_default_role (&videodec_class->cdata, "video_decoder.wmv");
}

static void
gst_omx_wmv_dec_init (GstOMXWMVDec * self)
{
  self->wvc1 = FALSE;
}

static gboolean
gst_omx_wmv_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state)
{
  return FALSE;
}

static gboolean
gst_omx_wmv_dec_set_format (GstOMXVideoDec * dec, GstOMXPort * port,
    GstVideoCodecState * state)
{
  gboolean ret;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;

#ifdef USE_OMX_TARGET_TEGRA
  GstBuffer *buf;
  gint size, width, height, index = 0;
  const gchar *format_string;
  GstMapInfo read_map = GST_MAP_INFO_INIT;
  GstMapInfo write_map = GST_MAP_INFO_INIT;
  GstOMXWMVDec *self = GST_OMX_WMV_DEC (dec);
  GstCaps *caps = state->caps;
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  format_string = gst_structure_get_string (structure, "format");
  size = gst_buffer_get_size (state->codec_data);

  if (!strcmp (format_string, "WVC1") || !strcmp (format_string, "wvc1")) {
    self->wvc1 = TRUE;
    buf = gst_buffer_make_writable (state->codec_data);
    /*openmax decoder want first byte to be skipped */
    gst_buffer_resize (buf, 1, -1);
    gst_buffer_replace (&state->codec_data, buf);
    g_print ("New buffer size = %zu\n", gst_buffer_get_size (buf));
  } else if (size > 3) {
    gst_buffer_map (state->codec_data, &read_map, GST_MAP_READ);
    if (read_map.data) {
      if (read_map.data[3] != 0xc5) {
        //Lets code Annex J and L of the SMPTE VC-1 specification
        char seq_l1[] = { 0xff, 0xff, 0xff, 0xc5 };
        char seq_l2[] = { 0x4, 0, 0, 0 };
        char seq_l3[] = { 0xc, 0, 0, 0 };

        gst_structure_get_int (structure, "width", &width);
        gst_structure_get_int (structure, "height", &height);

        buf = gst_buffer_new_allocate (NULL, size + 32, NULL);
        gst_buffer_map (buf, &write_map, GST_MAP_WRITE);
        memcpy (write_map.data + index, seq_l1, 4);
        index += 4;
        memcpy (write_map.data + index, seq_l2, 4);
        index += 4;
        memcpy (write_map.data + index, read_map.data, 4);
        index += 4;
        memcpy (write_map.data + index, &height, 4);
        index += 4;
        memcpy (write_map.data + index, &width, 4);
        index += 4;
        memcpy (write_map.data + index, seq_l3, 4);
        index += 4;
        memset (write_map.data + index, 0, 12);
        gst_buffer_replace (&state->codec_data, buf);
        gst_buffer_unmap (buf, &write_map);
      }
    }
    gst_buffer_unmap (state->codec_data, &read_map);
  }
#endif
  gst_omx_port_get_port_definition (port, &port_def);
  port_def.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
  ret = gst_omx_port_update_port_definition (port, &port_def) == OMX_ErrorNone;

  return ret;
}

#ifdef USE_OMX_TARGET_TEGRA
static GstFlowReturn
gst_omx_wmv_dec_prepare_frame (GstOMXVideoDec * dec, GstVideoCodecFrame * frame)
{
  GstOMXWMVDec *self = GST_OMX_WMV_DEC (dec);
  GstMemory *mem = NULL;
  GstMapInfo read_map = GST_MAP_INFO_INIT;
  GstMapInfo write_map = GST_MAP_INFO_INIT;
  guint32 start_code;

  if (self->wvc1) {
    //Handle WVC1 format
    gst_buffer_map (frame->input_buffer, &read_map, GST_MAP_READ);
    if (read_map.data) {
      start_code =
          (read_map.data[0] << 24) | (read_map.data[1] << 16) | (read_map.
          data[2] << 8) | read_map.data[3];
      gst_buffer_unmap (frame->input_buffer, &read_map);

      if (start_code != 0x10D && start_code != 0x10E) {
        mem = gst_allocator_alloc (NULL, 4, NULL);
        gst_memory_map (mem, &write_map, GST_MAP_WRITE);
        write_map.data[0] = 0;
        write_map.data[1] = 0;
        write_map.data[2] = 1;
        write_map.data[3] = 0xD;
        gst_memory_unmap (mem, &write_map);
        gst_buffer_prepend_memory (frame->input_buffer, mem);
      }
    }
  }
  return GST_FLOW_OK;
}
#endif
