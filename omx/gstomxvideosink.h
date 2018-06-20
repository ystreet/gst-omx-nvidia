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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GST_OMX_VIDEO_SINK_H_
#define _GST_OMX_VIDEO_SINK_H_

#include <gst/video/gstvideosink.h>
#include "gstomx.h"
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>

G_BEGIN_DECLS
#define GST_TYPE_OMX_VIDEO_SINK   (gst_omx_video_sink_get_type())
#define GST_OMX_VIDEO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_VIDEO_SINK,GstOmxVideoSink))
#define GST_OMX_VIDEO_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_VIDEO_SINK,GstOmxVideoSinkClass))
#define GST_OMX_VIDEO_SINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_OMX_VIDEO_SINK,GstOmxVideoSinkClass))
#define GST_IS_OMX_VIDEO_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_VIDEO_SINK))
#define GST_IS_OMX_VIDEO_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_VIDEO_SINK))
typedef struct _GstOmxVideoSink GstOmxVideoSink;
typedef struct _GstOmxVideoSinkClass GstOmxVideoSinkClass;

struct _GstOmxVideoSink
{
  GstVideoSink parent;

  GstOMXComponent *sink;
  GstOMXPort *sink_in_port;

  /* Framerate numerator and denominator */
  gint fps_n;
  gint fps_d;

  GstBufferPool *pool;
  GstBuffer *cur_buf;
  gboolean hw_path;

  guint overlay;
  guint overlay_depth;
  guint overlay_x;
  guint overlay_y;
  guint overlay_w;
  guint overlay_h;
  guint dc_head;
  guint profile;

  gboolean processing;
  gboolean update_pos;
  gboolean update_size;

  GMutex flow_lock;
};

struct _GstOmxVideoSinkClass
{
  GstVideoSinkClass parent_class;
  GstOMXClassData cdata;
};

GType gst_omx_video_sink_get_type (void);

G_END_DECLS
#endif
