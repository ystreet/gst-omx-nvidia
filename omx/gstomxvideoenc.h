/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (c) 2014-2017, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __GST_OMX_VIDEO_ENC_H__
#define __GST_OMX_VIDEO_ENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
#include <stdio.h>

#include "gstomx.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_VIDEO_ENC \
  (gst_omx_video_enc_get_type())
#define GST_OMX_VIDEO_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_VIDEO_ENC,GstOMXVideoEnc))
#define GST_OMX_VIDEO_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_VIDEO_ENC,GstOMXVideoEncClass))
#define GST_OMX_VIDEO_ENC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_OMX_VIDEO_ENC,GstOMXVideoEncClass))
#define GST_IS_OMX_VIDEO_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_VIDEO_ENC))
#define GST_IS_OMX_VIDEO_ENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_VIDEO_ENC))
typedef struct _GstOMXVideoEnc GstOMXVideoEnc;
typedef struct _GstOMXVideoEncClass GstOMXVideoEncClass;

struct _GstOMXVideoEnc
{
  GstVideoEncoder parent;

  GstBuffer *codec_data;

  /* < protected > */
  GstOMXComponent *enc;
  GstOMXPort *enc_in_port, *enc_out_port;

  /* < private > */
  GstVideoCodecState *input_state;
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
  gboolean hw_path;

  /* properties */
  guint32 control_rate;
  guint32 bitrate;
  guint32 peak_bitrate;
  guint32 quant_i_frames;
  guint32 quant_p_frames;
  guint32 quant_b_frames;
  guint32 iframeinterval;
  guint32 quality_level;
  gboolean SliceIntraRefreshEnable;
  gboolean bit_packetization;
  guint SliceIntraRefreshInterval;
  gboolean vbv_size_factor;
  guint32 temporal_tradeoff;
  guint32 max_frame_dist;
  gboolean EnableMVBufferMeta;
  guint32 MinQpI;
  guint32 MaxQpI;
  guint32 MinQpP;
  guint32 MaxQpP;
  guint32 MinQpB;
  guint32 MaxQpB;
  gboolean set_qpRange;
  gboolean measure_latency;
  guint64 framecount;
  FILE *tracing_file_enc;
  GQueue *got_frame_pt;
  gboolean EnableTwopassCBR;
  guint32 hw_preset_level;
  gboolean EnableStringentBitrate;

  GstFlowReturn downstream_flow_ret;
};

struct _GstOMXVideoEncClass
{
  GstVideoEncoderClass parent_class;

  GstOMXClassData cdata;

    gboolean (*set_format) (GstOMXVideoEnc * self, GstOMXPort * port,
      GstVideoCodecState * state);
  GstCaps *(*get_caps) (GstOMXVideoEnc * self, GstOMXPort * port,
      GstVideoCodecState * state);
    GstFlowReturn (*handle_output_frame) (GstOMXVideoEnc * self,
      GstOMXPort * port, GstOMXBuffer * buffer, GstVideoCodecFrame * frame);

  void (*force_IDR) (GstOMXVideoEnc *);
};

GType gst_omx_video_enc_get_type (void);

G_END_DECLS
#endif /* __GST_OMX_VIDEO_ENC_H__ */
