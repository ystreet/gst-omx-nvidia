/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (c) 2013 - 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __GST_OMX_VIDEO_DEC_H__
#define __GST_OMX_VIDEO_DEC_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>

#include "gstomx.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_VIDEO_DEC \
  (gst_omx_video_dec_get_type())
#define GST_OMX_VIDEO_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_VIDEO_DEC,GstOMXVideoDec))
#define GST_OMX_VIDEO_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_VIDEO_DEC,GstOMXVideoDecClass))
#define GST_OMX_VIDEO_DEC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_OMX_VIDEO_DEC,GstOMXVideoDecClass))
#define GST_IS_OMX_VIDEO_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_VIDEO_DEC))
#define GST_IS_OMX_VIDEO_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_VIDEO_DEC))
typedef struct _GstOMXVideoDec GstOMXVideoDec;
typedef struct _GstOMXVideoDecClass GstOMXVideoDecClass;


#ifdef USE_OMX_TARGET_TEGRA
typedef enum
{
  BUF_EGL,
  BUF_NVMM,
  BUF_NB
} NvBufType;
#endif

struct _GstOMXVideoDec
{
  GstVideoDecoder parent;

  /* < protected > */
  GstOMXComponent *dec;
  GstOMXPort *dec_in_port, *dec_out_port;

  GstBufferPool *in_port_pool, *out_port_pool;

  /* < private > */
  GstVideoCodecState *input_state;
  GstBuffer *codec_data;
  /* TRUE if the component is configured and saw
   * the first buffer */
  gboolean started;

  GstClockTime last_upstream_ts;

  /* Draining state */
  GMutex drain_lock;
  GCond drain_cond;
  /* TRUE if EOS buffers shouldn't be forwarded */
  gboolean draining;

  /* TRUE if upstream is EOS */
  gboolean eos;

  GstFlowReturn downstream_flow_ret;

#ifdef USE_OMX_TARGET_TEGRA
  gboolean use_omxdec_res;
  gboolean full_frame_data;
  gboolean disable_dpb;
  guint32 skip_frames;
  guint32 output_buffers;
  gboolean enable_error_check;
  gboolean enable_frame_type_reporting;
  gboolean cpu_dec_buf;
#endif

#ifdef USE_OMX_TARGET_RPI
  GstOMXComponent *egl_render;
  GstOMXPort *egl_in_port, *egl_out_port;
#endif

#if defined (USE_OMX_TARGET_RPI) || defined (USE_OMX_TARGET_TEGRA)
  gboolean eglimage;
#endif
};

struct _GstOMXVideoDecClass
{
  GstVideoDecoderClass parent_class;

  GstOMXClassData cdata;

    gboolean (*is_format_change) (GstOMXVideoDec * self, GstOMXPort * port,
      GstVideoCodecState * state);
    gboolean (*set_format) (GstOMXVideoDec * self, GstOMXPort * port,
      GstVideoCodecState * state);
    GstFlowReturn (*prepare_frame) (GstOMXVideoDec * self,
      GstVideoCodecFrame * frame);
    void (*video_dec_loop) (GstOMXBuffer *buf);
};

GType gst_omx_video_dec_get_type (void);

OMX_ERRORTYPE gst_omx_set_full_frame_data_property (OMX_HANDLETYPE omx_handle);

typedef enum
{
  GST_DECODE_ALL,
  GST_SKIP_NON_REF_FRAMES,
  GST_DECODE_KEY_FRAMES
} GstVideoSkipFrames;

G_END_DECLS
#endif /* __GST_OMX_VIDEO_DEC_H__ */
