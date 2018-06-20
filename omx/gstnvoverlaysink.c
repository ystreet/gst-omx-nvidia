/* GStreamer
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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
#include "gstnvoverlaysink.h"

GST_DEBUG_CATEGORY_STATIC (gst_nv_overlay_sink_debug_category);
#define GST_CAT_DEFAULT gst_nv_overlay_sink_debug_category

/* prototypes */

static void gst_nv_overlay_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_nv_overlay_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
  PROP_DISPLAY
};

#define DEFAULT_DC_HEAD        0

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstNvOverlaySink, gst_nv_overlay_sink,
    GST_TYPE_OMX_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT (gst_nv_overlay_sink_debug_category,
        "nvoverlaysink", 0, "debug category for nvoverlaysink element"));

static void
gst_nv_overlay_sink_class_init (GstNvOverlaySinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstOmxVideoSinkClass *videosink_class = GST_OMX_VIDEO_SINK_CLASS (klass);

  videosink_class->cdata.default_sink_template_caps = "video/x-raw(memory:NVMM), "
    "width = (int) [ 1, max ] , "
    "height = (int) [ 1, max ] , " "framerate = (fraction) [ 0, max ];"
    "video/x-raw, "
    "width = (int) [ 1, max ] , "
    "height = (int) [ 1, max ] , " "framerate = (fraction) [ 0, max ] ";


  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "OpenMax Video Sink", "Sink/Video", "Renders Video",
      "Jitendra Kumar <jitendrak@nvidia.com>");

  gobject_class->set_property = gst_nv_overlay_sink_set_property;
  gobject_class->get_property = gst_nv_overlay_sink_get_property;

  /*
   *  Display Id
   */
  g_object_class_install_property (gobject_class, PROP_DISPLAY,
      g_param_spec_uint ("display-id", "Display Id",
          "Display on which overlay should be rendered", 0, G_MAXUINT,
          DEFAULT_DC_HEAD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_nv_overlay_sink_init (GstNvOverlaySink * nvoverlaysink)
{
  GstOmxVideoSink *omxvideosink = GST_OMX_VIDEO_SINK (nvoverlaysink);
  omxvideosink->dc_head = DEFAULT_DC_HEAD;
}

void
gst_nv_overlay_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  /* GstNvOverlaySink *nvoverlaysink = GST_NV_OVERLAY_SINK (object); */
  GstOmxVideoSink *omxvideosink = GST_OMX_VIDEO_SINK (object);

  switch (property_id) {
    case PROP_DISPLAY:
      omxvideosink->dc_head = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_nv_overlay_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  /* GstNvOverlaySink *nvoverlaysink = GST_NV_OVERLAY_SINK (object); */
  GstOmxVideoSink *omxvideosink = GST_OMX_VIDEO_SINK (object);

  switch (property_id) {
    case PROP_DISPLAY:
      g_value_set_uint (value, omxvideosink->dc_head);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}
