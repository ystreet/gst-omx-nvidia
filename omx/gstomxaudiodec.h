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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_OMX_AUDIO_DEC_H_
#define _GST_OMX_AUDIO_DEC_H_

#include <gst/audio/gstaudiodecoder.h>
#include "gstomx.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_AUDIO_DEC   (gst_omx_audio_dec_get_type())
#define GST_OMX_AUDIO_DEC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_AUDIO_DEC,GstOMXAudioDec))
#define GST_OMX_AUDIO_DEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_AUDIO_DEC,GstOMXAudioDecClass))
#define GST_OMX_AUDIO_DEC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_OMX_AUDIO_DEC,GstOMXAudioDecClass))
#define GST_IS_OMX_AUDIO_DEC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_AUDIO_DEC))
#define GST_IS_OMX_AUDIO_DEC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_AUDIO_DEC))
typedef struct _GstOMXAudioDec GstOMXAudioDec;
typedef struct _GstOMXAudioDecClass GstOMXAudioDecClass;

struct _GstOMXAudioDec
{
  GstAudioDecoder parent;

  GstOMXComponent *dec;
  GstOMXPort *dec_in_port, *dec_out_port;

  GstBuffer *codec_data;
  GstCaps *last_caps;
  gint rate, channel;

  /* Draining state */
  GMutex drain_lock;
  GCond drain_cond;
  /* TRUE if EOS buffers shouldn't be forwarded */
  gboolean draining;

  gboolean started;
  gboolean eos;
  GstClockTime last_upstream_ts;
  GstFlowReturn downstream_flow_ret;

};

struct _GstOMXAudioDecClass
{
  GstAudioDecoderClass parent_class;
  GstOMXClassData cdata;

    gboolean (*set_format) (GstOMXAudioDec * self, GstOMXPort * port,
      GstCaps * state);
};

GType gst_omx_audio_dec_get_type (void);

G_END_DECLS
#endif
