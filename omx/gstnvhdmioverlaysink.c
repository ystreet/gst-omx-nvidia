/* GStreamer
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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
#include <gst/video/gstvideosink.h>
#include "gstnvhdmioverlaysink.h"

GST_DEBUG_CATEGORY_STATIC (gst_nv_hdmi_overlay_sink_debug_category);
#define GST_CAT_DEFAULT gst_nv_hdmi_overlay_sink_debug_category

/* prototypes */

static void gst_nv_hdmi_overlay_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_nv_hdmi_overlay_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstNvHDMIOverlaySink, gst_nv_hdmi_overlay_sink,
    GST_TYPE_OMX_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT (gst_nv_hdmi_overlay_sink_debug_category,
        "nvhdmioverlaysink", 0,
        "debug category for nvhdmioverlaysink element"));

static void
gst_nv_hdmi_overlay_sink_class_init (GstNvHDMIOverlaySinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstOmxVideoSinkClass *videosink_class = GST_OMX_VIDEO_SINK_CLASS (klass);

  videosink_class->cdata.default_sink_template_caps = "video/x-raw(memory:NVMM), "
      "width = (int) [ 1, max ] , "
      "height = (int) [ 1, max ] , " "framerate = (fraction) [ 0, max ];"
      "video/x-raw, "
      "width = (int) [ 1, max ] , "
      "height = (int) [ 1, max ] , " "framerate = (fraction) [ 0, max ]";

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "OpenMax HDMI Video Sink", "Sink/Video",
      "Renders Video to HDMI (Deprecated - Use nvoverlaysink instead)",
      "Jitendra Kumar <jitendrak@nvidia.com>");

  gobject_class->set_property = gst_nv_hdmi_overlay_sink_set_property;
  gobject_class->get_property = gst_nv_hdmi_overlay_sink_get_property;
}

static void
gst_nv_hdmi_overlay_sink_init (GstNvHDMIOverlaySink * nvhdmioverlaysink)
{
  GstOmxVideoSink *omxvideosink = GST_OMX_VIDEO_SINK (nvhdmioverlaysink);
  omxvideosink->dc_head = 1;
}

void
gst_nv_hdmi_overlay_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  /* GstNvHDMIOverlaySink *nvhdmioverlaysink = GST_NV_HDMI_OVERLAY_SINK (object); */

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_nv_hdmi_overlay_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  /* GstNvHDMIOverlaySink *nvhdmioverlaysink = GST_NV_HDMI_OVERLAY_SINK (object); */

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}
