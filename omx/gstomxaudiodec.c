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
#include <gst/audio/gstaudiodecoder.h>
#include "gstomxaudiodec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_audio_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_audio_dec_debug_category

/* prototypes */

static void gst_omx_audio_dec_finalize (GObject * object);

static gboolean gst_omx_audio_dec_start (GstAudioDecoder * decoder);
static gboolean gst_omx_audio_dec_stop (GstAudioDecoder * decoder);
static gboolean gst_omx_audio_dec_set_format (GstAudioDecoder * decoder,
    GstCaps * caps);
static GstFlowReturn gst_omx_audio_dec_handle_frame (GstAudioDecoder * decoder,
    GstBuffer * buffer);
static gboolean gst_omx_audio_dec_open (GstAudioDecoder * decoder);
static gboolean gst_omx_audio_dec_close (GstAudioDecoder * decoder);
static gboolean gst_omx_audio_dec_shutdown (GstOMXAudioDec * self);

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstOMXAudioDec, gst_omx_audio_dec,
    GST_TYPE_AUDIO_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_omx_audio_dec_debug_category, "omxaudiodec", 0,
        "debug category for omxaudiodec element"));


static GstStateChangeReturn
gst_omx_audio_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstOMXAudioDec *self;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  g_return_val_if_fail (GST_IS_OMX_AUDIO_DEC (element),
      GST_STATE_CHANGE_FAILURE);
  self = GST_OMX_AUDIO_DEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->downstream_flow_ret = GST_FLOW_OK;
      self->draining = FALSE;
      self->started = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (self->dec_in_port)
        gst_omx_port_set_flushing (self->dec_in_port, 5 * GST_SECOND, TRUE);
      if (self->dec_out_port)
        gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, TRUE);
      g_mutex_lock (&self->drain_lock);
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
      g_mutex_unlock (&self->drain_lock);
      break;
    default:
      break;
  }

  ret =
      GST_ELEMENT_CLASS (gst_omx_audio_dec_parent_class)->change_state
      (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->downstream_flow_ret = GST_FLOW_FLUSHING;
      self->started = FALSE;

      if (!gst_omx_audio_dec_shutdown (self))
        ret = GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_omx_audio_dec_class_init (GstOMXAudioDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAudioDecoderClass *audiodec_class = GST_AUDIO_DECODER_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  klass->cdata.type = GST_OMX_COMPONENT_TYPE_FILTER;
  klass->cdata.default_src_template_caps = "audio/x-raw, "
      "layout=(string) interleaved, " "format=(string) S16LE";

  gobject_class->finalize = gst_omx_audio_dec_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_audio_dec_change_state);

  audiodec_class->start = GST_DEBUG_FUNCPTR (gst_omx_audio_dec_start);
  audiodec_class->stop = GST_DEBUG_FUNCPTR (gst_omx_audio_dec_stop);
  audiodec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_audio_dec_set_format);
  audiodec_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_omx_audio_dec_handle_frame);
  audiodec_class->open = GST_DEBUG_FUNCPTR (gst_omx_audio_dec_open);
  audiodec_class->close = GST_DEBUG_FUNCPTR (gst_omx_audio_dec_close);
}

static void
gst_omx_audio_dec_init (GstOMXAudioDec * self)
{
  g_mutex_init (&self->drain_lock);
  g_cond_init (&self->drain_cond);
}

void
gst_omx_audio_dec_finalize (GObject * object)
{
  GstOMXAudioDec *self = GST_OMX_AUDIO_DEC (object);
  g_mutex_clear (&self->drain_lock);
  g_cond_clear (&self->drain_cond);

  G_OBJECT_CLASS (gst_omx_audio_dec_parent_class)->finalize (object);
}

static gboolean
gst_omx_audio_dec_start (GstAudioDecoder * decoder)
{
  GstOMXAudioDec *self = GST_OMX_AUDIO_DEC (decoder);

  self->last_upstream_ts = 0;
  self->eos = FALSE;
  self->downstream_flow_ret = GST_FLOW_OK;

  return TRUE;
}

static gboolean
gst_omx_audio_dec_stop (GstAudioDecoder * decoder)
{
  GstOMXAudioDec *self = GST_OMX_AUDIO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Stopping Decoder");

  gst_omx_port_set_flushing (self->dec_in_port, 5 * GST_SECOND, TRUE);
  gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, TRUE);

  gst_pad_stop_task (GST_AUDIO_DECODER_SRC_PAD (decoder));

  if (gst_omx_component_get_state (self->dec, 0) > OMX_StateIdle)
    gst_omx_component_set_state (self->dec, OMX_StateIdle);

  self->downstream_flow_ret = GST_FLOW_FLUSHING;
  self->started = FALSE;
  self->eos = FALSE;

  g_mutex_lock (&self->drain_lock);
  self->draining = FALSE;
  g_cond_broadcast (&self->drain_cond);
  g_mutex_unlock (&self->drain_lock);

  gst_omx_component_get_state (self->dec, 5 * GST_SECOND);

  if (self->codec_data)
    gst_buffer_replace (&self->codec_data, NULL);

  if (self->last_caps)
    gst_caps_unref (self->last_caps);

  GST_DEBUG_OBJECT (self, "Stopped decoder");

  return TRUE;
}

static GstFlowReturn
gst_omx_audio_dec_drain (GstOMXAudioDec * self, gboolean is_eos)
{
  GstOMXAudioDecClass *klass;
  GstOMXBuffer *buf;
  GstOMXAcquireBufferReturn acq_ret;
  OMX_ERRORTYPE err;

  GST_DEBUG_OBJECT (self, "Draining component");
  klass = GST_OMX_AUDIO_DEC_GET_CLASS (self);

  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Component not started yet");
    return GST_FLOW_OK;
  }
  self->started = FALSE;

  if (self->eos) {
    GST_DEBUG_OBJECT (self, "Component is EOS already");
    return GST_FLOW_OK;
  }
  if (is_eos)
    self->eos = TRUE;

  if ((klass->cdata.hacks & GST_OMX_HACK_NO_EMPTY_EOS_BUFFER)) {
    GST_WARNING_OBJECT (self, "Component does not support empty EOS buffers");
    return GST_FLOW_OK;
  }

  GST_AUDIO_DECODER_STREAM_UNLOCK (self);

  acq_ret = gst_omx_port_acquire_buffer (self->dec_in_port, &buf);
  if (acq_ret != GST_OMX_ACQUIRE_BUFFER_OK) {
    GST_AUDIO_DECODER_STREAM_LOCK (self);
    GST_ERROR_OBJECT (self, "Failed to acquire buffer for draining: %d",
        acq_ret);
    return GST_FLOW_ERROR;
  }

  g_mutex_lock (&self->drain_lock);
  self->draining = TRUE;
  buf->omx_buf->nFilledLen = 0;
  buf->omx_buf->nTimeStamp =
      gst_util_uint64_scale (self->last_upstream_ts, OMX_TICKS_PER_SECOND,
      GST_SECOND);
  buf->omx_buf->nTickCount = 0;
  buf->omx_buf->nFlags |= OMX_BUFFERFLAG_EOS;
  err = gst_omx_port_release_buffer (self->dec_in_port, buf);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Failed to drain component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    GST_AUDIO_DECODER_STREAM_LOCK (self);
    return GST_FLOW_ERROR;
  }

  GST_DEBUG_OBJECT (self, "Waiting until component is drained");

  if (G_UNLIKELY (self->dec->hacks & GST_OMX_HACK_DRAIN_MAY_NOT_RETURN)) {
    gint64 wait_until = g_get_monotonic_time () + G_TIME_SPAN_SECOND / 2;

    if (!g_cond_wait_until (&self->drain_cond, &self->drain_lock, wait_until))
      GST_WARNING_OBJECT (self, "Drain timed out");
    else
      GST_DEBUG_OBJECT (self, "Drained component");

  } else {
    g_cond_wait (&self->drain_cond, &self->drain_lock);
    GST_DEBUG_OBJECT (self, "Drained component");
  }

  g_mutex_unlock (&self->drain_lock);
  GST_AUDIO_DECODER_STREAM_LOCK (self);

  self->started = FALSE;

  return GST_FLOW_OK;
}

static OMX_ERRORTYPE
gst_omx_audio_dec_set_output_format (GstOMXAudioDec * self)
{
  gint width, i;
  gint endianess;
  gboolean signed_data;
  GstOMXPort *port;
  GstAudioInfo info;
  OMX_AUDIO_PARAM_PCMMODETYPE pcm_mode;
  GstAudioFormat format;
  GstAudioChannelPosition *position = NULL;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  port = self->dec_out_port;

  GST_AUDIO_DECODER_STREAM_LOCK (self);

  GST_OMX_INIT_STRUCT (&pcm_mode);
  pcm_mode.nPortIndex = port->index;

  err = gst_omx_component_get_parameter (self->dec, OMX_IndexParamAudioPcm,
      &pcm_mode);

  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to get PCM parameters from component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    goto done;
  }

  self->channel = pcm_mode.nChannels;
  self->rate = pcm_mode.nSamplingRate;
  width = pcm_mode.nBitPerSample;
  endianess = pcm_mode.eEndian & OMX_EndianBig ? G_BIG_ENDIAN : G_LITTLE_ENDIAN;
  signed_data = pcm_mode.eNumData == OMX_NumericalDataSigned ? TRUE : FALSE;

  position = g_new0 (GstAudioChannelPosition, self->channel);

  for (i = 0; i < self->channel; i++) {
    switch (pcm_mode.eChannelMapping[i]) {

      case OMX_AUDIO_ChannelNone:
        position[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
        break;
      case OMX_AUDIO_ChannelLF:
        position[i] = GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT;
        break;
      case OMX_AUDIO_ChannelRF:
        position[i] = GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT;
        break;
      case OMX_AUDIO_ChannelCF:
        position[i] = GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER;
        break;
      case OMX_AUDIO_ChannelLS:
        position[i] = GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT;
        break;
      case OMX_AUDIO_ChannelRS:
        position[i] = GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT;
        break;
      case OMX_AUDIO_ChannelLFE:
        position[i] = GST_AUDIO_CHANNEL_POSITION_LFE1;
        break;
      case OMX_AUDIO_ChannelCS:
        position[i] = GST_AUDIO_CHANNEL_POSITION_REAR_CENTER;
        break;
      case OMX_AUDIO_ChannelLR:
        position[i] = GST_AUDIO_CHANNEL_POSITION_REAR_LEFT;
        break;
      case OMX_AUDIO_ChannelRR:
        position[i] = GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT;
        break;
      default:
        break;
    }

  }

  format =
      gst_audio_format_build_integer (signed_data, endianess, width, width);

  gst_audio_info_set_format (&info, format, self->rate, self->channel,
      position);
  gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (self), &info);

  if (position)
    g_free (position);

  GST_AUDIO_DECODER_STREAM_UNLOCK (self);

done:
  return err;
}

static OMX_ERRORTYPE
gst_omx_audio_dec_reconfigure_output_port (GstOMXAudioDec * self)
{

  GstOMXPort *port;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  port = self->dec_out_port;

  err = gst_omx_audio_dec_set_output_format (self);
  if (err != OMX_ErrorNone)
    goto done;

  if (!gst_omx_port_is_enabled (port)) {
    err = gst_omx_port_set_enabled (port, TRUE);
    if (err != OMX_ErrorNone) {
      GST_INFO_OBJECT (self,
          "Failed to enable port: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      goto done;
    }
  }

  err = gst_omx_port_allocate_buffers (port);
  if (err != OMX_ErrorNone)
    goto done;

  err = gst_omx_port_wait_enabled (port, 5 * GST_SECOND);
  if (err != OMX_ErrorNone)
    goto done;

  err = gst_omx_port_populate (port);
  if (err != OMX_ErrorNone)
    goto done;

  err = gst_omx_port_mark_reconfigured (port);
  if (err != OMX_ErrorNone)
    goto done;

done:
  return err;
}

static void
gst_omx_audio_dec_loop (GstOMXAudioDec * self)
{
  GstOMXPort *port;
  GstOMXBuffer *buf = NULL;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstOMXAcquireBufferReturn acq_return;
  OMX_ERRORTYPE err;

  port = self->dec_out_port;
  acq_return = gst_omx_port_acquire_buffer (port, &buf);
  if (acq_return == GST_OMX_ACQUIRE_BUFFER_ERROR) {
    goto component_error;
  } else if (acq_return == GST_OMX_ACQUIRE_BUFFER_FLUSHING) {
    goto flushing;
  } else if (acq_return == GST_OMX_ACQUIRE_BUFFER_EOS) {
    goto eos;
  }

  if (!gst_pad_has_current_caps (GST_AUDIO_DECODER_SRC_PAD (self)) ||
      acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {

    GST_DEBUG_OBJECT (self, "Port settings have changed, updating caps");

    /* Reallocate all buffers */
    if (acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE
        && gst_omx_port_is_enabled (port)) {
      err = gst_omx_port_set_enabled (port, FALSE);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
      err = gst_omx_port_wait_buffers_released (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
      err = gst_omx_port_deallocate_buffers (port);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
      err = gst_omx_port_wait_enabled (port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
    }

    if (acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {
      err = gst_omx_audio_dec_reconfigure_output_port (self);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
    } else {
      err = gst_omx_audio_dec_set_output_format (self);
      if (err != OMX_ErrorNone)
        goto component_error;
    }

    /* Now get a buffer */
    if (acq_return != GST_OMX_ACQUIRE_BUFFER_OK) {
      return;
    }
  }

  g_assert (acq_return == GST_OMX_ACQUIRE_BUFFER_OK);

  if (gst_omx_port_is_flushing (port)) {
    GST_DEBUG_OBJECT (self, "Flushing");
    gst_omx_port_release_buffer (port, buf);
    goto flushing;
  }

  if (buf)
    GST_DEBUG_OBJECT (self, "Handling buffer: 0x%08x %" G_GUINT64_FORMAT,
        (guint) buf->omx_buf->nFlags, (guint64) buf->omx_buf->nTimeStamp);

  GST_AUDIO_DECODER_STREAM_LOCK (self);

  if (buf && buf->omx_buf->nFilledLen > 0) {
    GstBuffer *outbuf;
    GstMapInfo map = GST_MAP_INFO_INIT;

    GST_DEBUG_OBJECT (self, "Handling output data");

    outbuf =
        gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (self),
        buf->omx_buf->nFilledLen);

    gst_buffer_map (outbuf, &map, GST_MAP_WRITE);

    if (map.data)
      memcpy (map.data,
          buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
          buf->omx_buf->nFilledLen);
    gst_buffer_unmap (outbuf, &map);

    GST_BUFFER_TIMESTAMP (outbuf) =
        gst_util_uint64_scale (buf->omx_buf->nTimeStamp, GST_SECOND,
        OMX_TICKS_PER_SECOND);
    if (buf->omx_buf->nTickCount != 0)
      GST_BUFFER_DURATION (outbuf) =
          gst_util_uint64_scale (buf->omx_buf->nTickCount, GST_SECOND,
          OMX_TICKS_PER_SECOND);

    flow_ret =
        gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (self), outbuf, 1);

    GST_DEBUG_OBJECT (self, "Finished frame: %s", gst_flow_get_name (flow_ret));
  }

  if (buf) {
    err = gst_omx_port_release_buffer (port, buf);
    if (err != OMX_ErrorNone)
      goto release_error;
  }

  self->downstream_flow_ret = flow_ret;

  if (flow_ret != GST_FLOW_OK)
    goto flow_error;

  GST_AUDIO_DECODER_STREAM_UNLOCK (self);

  return;

component_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("OpenMAX component in error state %s (0x%08x)",
            gst_omx_component_get_last_error_string (self->dec),
            gst_omx_component_get_last_error (self->dec)));
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    self->started = FALSE;
    return;
  }

flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_FLUSHING;
    self->started = FALSE;
    return;
  }

eos:
  {
    g_mutex_lock (&self->drain_lock);
    if (self->draining) {
      GST_DEBUG_OBJECT (self, "Drained");
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
      flow_ret = GST_FLOW_OK;
      gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    } else {
      GST_DEBUG_OBJECT (self, "Component signalled EOS");
      flow_ret = GST_FLOW_EOS;
    }
    g_mutex_unlock (&self->drain_lock);

    GST_AUDIO_DECODER_STREAM_LOCK (self);
    self->downstream_flow_ret = flow_ret;

    /* Here we fallback and pause the task for the EOS case */
    if (flow_ret != GST_FLOW_OK)
      goto flow_error;

    GST_AUDIO_DECODER_STREAM_UNLOCK (self);

    return;
  }

flow_error:
  {
    if (flow_ret == GST_FLOW_EOS) {
      GST_DEBUG_OBJECT (self, "EOS");

      gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    } else if (flow_ret == GST_FLOW_NOT_LINKED || flow_ret < GST_FLOW_EOS) {
      GST_ELEMENT_ERROR (self, STREAM, FAILED,
          ("Internal data stream error."), ("stream stopped, reason %s",
              gst_flow_get_name (flow_ret)));

      gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    }
    self->started = FALSE;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    return;
  }

reconfigure_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Unable to reconfigure output port"));
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    self->started = FALSE;
    return;
  }
release_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Failed to relase output buffer to component: %s (0x%08x)",
            gst_omx_error_to_string (err), err));
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    self->started = FALSE;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    return;
  }
}

static gboolean
gst_omx_audio_dec_set_format (GstAudioDecoder * decoder, GstCaps * caps)
{
  GstOMXAudioDec *self = GST_OMX_AUDIO_DEC (decoder);
  GstOMXAudioDecClass *klass = GST_OMX_AUDIO_DEC_GET_CLASS (decoder);
  gboolean needs_disable = FALSE;
  GstStructure *structure;
  const GValue *codec_data;

  GST_DEBUG_OBJECT (self, "Setting new caps %" GST_PTR_FORMAT, caps);

  if (self->last_caps && gst_caps_is_equal (self->last_caps, caps)) {
    GST_DEBUG_OBJECT (self, "same caps");
    return TRUE;
  }

  needs_disable = gst_omx_component_get_state (self->dec,
      GST_CLOCK_TIME_NONE) != OMX_StateLoaded;

  if (needs_disable) {

    GST_DEBUG_OBJECT (self, "Need to disable and drain Decoder");

    gst_omx_audio_dec_drain (self, FALSE);
    gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, TRUE);

    /* Wait until the srcpad loop is finished,
     * unlock GST_AUDIO_DECODER_STREAM_LOCK to prevent deadlocks
     * caused by using this lock from inside the loop function */
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    gst_pad_stop_task (GST_AUDIO_DECODER_SRC_PAD (decoder));
    GST_AUDIO_DECODER_STREAM_LOCK (self);

    if (gst_omx_port_set_enabled (self->dec_in_port, FALSE) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_set_enabled (self->dec_out_port, FALSE) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_buffers_released (self->dec_in_port,
            5 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_buffers_released (self->dec_out_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_deallocate_buffers (self->dec_in_port) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_deallocate_buffers (self->dec_out_port) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_enabled (self->dec_in_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_enabled (self->dec_out_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;

    GST_DEBUG_OBJECT (self, "Decoder drained and disabled");
  }

  if (self->codec_data)
    gst_buffer_replace (&self->codec_data, NULL);

  structure = gst_caps_get_structure (caps, 0);
  codec_data = gst_structure_get_value (structure, "codec_data");

  if (codec_data && G_VALUE_TYPE (codec_data) == GST_TYPE_BUFFER)
    self->codec_data = GST_BUFFER (g_value_dup_boxed (codec_data));

  if (!gst_structure_get_int (structure, "rate", &self->rate)) {
    GST_WARNING_OBJECT (self, "rate is empty");
    self->rate = 44100;
  }

  if (!gst_structure_get_int (structure, "channels", &self->channel)) {
    GST_WARNING_OBJECT (self, "channel is not set");
    self->channel = 2;
  }

  if (klass->set_format) {
    if (!klass->set_format (self, self->dec_in_port, caps)) {
      GST_ERROR_OBJECT (self, "Subclass failed to set the new format");
      return FALSE;
    }
  }

  GST_DEBUG_OBJECT (self, "Enabling component");

  if (needs_disable) {
    if (gst_omx_port_set_enabled (self->dec_in_port, TRUE) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_allocate_buffers (self->dec_in_port) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_enabled (self->dec_in_port,
            5 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_mark_reconfigured (self->dec_in_port) != OMX_ErrorNone)
      return FALSE;

  } else {
    /* Disable output port */
    if (gst_omx_port_set_enabled (self->dec_out_port, FALSE) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_port_wait_enabled (self->dec_out_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_component_set_state (self->dec, OMX_StateIdle) != OMX_ErrorNone)
      return FALSE;

    /* Need to allocate buffers to reach Idle state */
    if (gst_omx_port_allocate_buffers (self->dec_in_port) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_component_get_state (self->dec,
            GST_CLOCK_TIME_NONE) != OMX_StateIdle)
      return FALSE;

    if (gst_omx_component_set_state (self->dec,
            OMX_StateExecuting) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_component_get_state (self->dec,
            GST_CLOCK_TIME_NONE) != OMX_StateExecuting)
      return FALSE;
  }

#ifdef USE_OMX_TARGET_TEGRA
  /* PortSetting change event comes only if output port has buffers allocated */
  if (gst_omx_audio_dec_reconfigure_output_port (self) != OMX_ErrorNone)
    return FALSE;
#endif

  /* Unset flushing to allow ports to accept data again */
  gst_omx_port_set_flushing (self->dec_in_port, 5 * GST_SECOND, FALSE);
  gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, FALSE);

  if (gst_omx_component_get_last_error (self->dec) != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Component in error state: %s (0x%08x)",
        gst_omx_component_get_last_error_string (self->dec),
        gst_omx_component_get_last_error (self->dec));
    return FALSE;
  }

  /* Start the srcpad loop again */
  GST_DEBUG_OBJECT (self, "Starting task again");

  self->downstream_flow_ret = GST_FLOW_OK;
  gst_pad_start_task (GST_AUDIO_DECODER_SRC_PAD (self),
      (GstTaskFunction) gst_omx_audio_dec_loop, decoder, NULL);

  return TRUE;
}

static GstFlowReturn
gst_omx_audio_dec_handle_frame (GstAudioDecoder * decoder, GstBuffer * buffer)
{
  GstOMXAcquireBufferReturn acq_ret = GST_OMX_ACQUIRE_BUFFER_ERROR;
  GstOMXAudioDec *self;
  GstOMXPort *port;
  GstOMXBuffer *buf;
  GstBuffer *codec_data = NULL;
  guint offset = 0, size;
  GstClockTime timestamp, duration;
  OMX_ERRORTYPE err;

  self = GST_OMX_AUDIO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Handling frame");

  if (self->eos) {
    GST_WARNING_OBJECT (self, "Got frame after EOS");
    return GST_FLOW_EOS;
  }

  if (buffer == NULL)
    return GST_FLOW_OK;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);

  if (self->downstream_flow_ret != GST_FLOW_OK) {
    return self->downstream_flow_ret;
  }

  port = self->dec_in_port;

  size = gst_buffer_get_size (buffer);
  while (offset < size) {
    /* Make sure to release the base class stream lock, otherwise
     * _loop() can't call _finish_frame() and we might block forever
     * because no input buffers are released */
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    acq_ret = gst_omx_port_acquire_buffer (port, &buf);

    if (acq_ret == GST_OMX_ACQUIRE_BUFFER_ERROR) {
      GST_AUDIO_DECODER_STREAM_LOCK (self);
      goto component_error;
    } else if (acq_ret == GST_OMX_ACQUIRE_BUFFER_FLUSHING) {
      GST_AUDIO_DECODER_STREAM_LOCK (self);
      goto flushing;
    } else if (acq_ret == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {
      /* Reallocate all buffers */
      err = gst_omx_port_set_enabled (port, FALSE);
      if (err != OMX_ErrorNone) {
        GST_AUDIO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_buffers_released (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_AUDIO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_deallocate_buffers (port);
      if (err != OMX_ErrorNone) {
        GST_AUDIO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_enabled (port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_AUDIO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_set_enabled (port, TRUE);
      if (err != OMX_ErrorNone) {
        GST_AUDIO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_allocate_buffers (port);
      if (err != OMX_ErrorNone) {
        GST_AUDIO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_enabled (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_AUDIO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_mark_reconfigured (port);
      if (err != OMX_ErrorNone) {
        GST_AUDIO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      /* Now get a new buffer and fill it */
      GST_AUDIO_DECODER_STREAM_LOCK (self);
      continue;
    }

    GST_AUDIO_DECODER_STREAM_LOCK (self);

    g_assert (acq_ret == GST_OMX_ACQUIRE_BUFFER_OK && buf != NULL);

    if (buf->omx_buf->nAllocLen - buf->omx_buf->nOffset <= 0) {
      gst_omx_port_release_buffer (port, buf);
      goto full_buffer;
    }

    if (self->downstream_flow_ret != GST_FLOW_OK) {
      gst_omx_port_release_buffer (port, buf);
      return self->downstream_flow_ret;
    }

    if (self->codec_data) {
      GST_DEBUG_OBJECT (self, "Passing codec data to the component");

      codec_data = self->codec_data;

      if (buf->omx_buf->nAllocLen - buf->omx_buf->nOffset <
          gst_buffer_get_size (codec_data)) {
        gst_omx_port_release_buffer (port, buf);
        goto too_large_codec_data;
      }

      buf->omx_buf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
      buf->omx_buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
      buf->omx_buf->nFilledLen = gst_buffer_get_size (codec_data);;
      gst_buffer_extract (codec_data, 0,
          buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
          buf->omx_buf->nFilledLen);

      if (GST_CLOCK_TIME_IS_VALID (timestamp))
        buf->omx_buf->nTimeStamp =
            gst_util_uint64_scale (timestamp, OMX_TICKS_PER_SECOND, GST_SECOND);
      else
        buf->omx_buf->nTimeStamp = 0;
      buf->omx_buf->nTickCount = 0;

      self->started = TRUE;
      err = gst_omx_port_release_buffer (port, buf);
      gst_buffer_replace (&self->codec_data, NULL);

      if (err != OMX_ErrorNone)
        goto release_error;
      /* Acquire new buffer for the actual frame */
      continue;
    }

    /* Now handle the frame */
    GST_DEBUG_OBJECT (self, "Passing frame offset %d to the component", offset);

    /* Copy the buffer content in chunks of size as requested
     * by the port */
    buf->omx_buf->nFilledLen =
        MIN (size - offset, buf->omx_buf->nAllocLen - buf->omx_buf->nOffset);
    gst_buffer_extract (buffer, offset,
        buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
        buf->omx_buf->nFilledLen);

    if (timestamp != GST_CLOCK_TIME_NONE) {
      buf->omx_buf->nTimeStamp =
          gst_util_uint64_scale (timestamp, OMX_TICKS_PER_SECOND, GST_SECOND);
      self->last_upstream_ts = timestamp;
    } else {
      buf->omx_buf->nTimeStamp = 0;
    }

    if (duration != GST_CLOCK_TIME_NONE && offset == 0) {
      buf->omx_buf->nTickCount =
          gst_util_uint64_scale (buf->omx_buf->nFilledLen, duration, size);
      self->last_upstream_ts += duration;
    } else {
      buf->omx_buf->nTickCount = 0;
    }

    offset += buf->omx_buf->nFilledLen;

    if (offset == size)
      buf->omx_buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

    self->started = TRUE;
    err = gst_omx_port_release_buffer (port, buf);
    if (err != OMX_ErrorNone)
      goto release_error;
  }

  GST_DEBUG_OBJECT (self, "Passed frame to component");

  return self->downstream_flow_ret;

full_buffer:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("Got OpenMAX buffer with no free space (%p, %u/%u)", buf,
            (guint) buf->omx_buf->nOffset, (guint) buf->omx_buf->nAllocLen));
    return GST_FLOW_ERROR;
  }

too_large_codec_data:
  {
    GST_ELEMENT_ERROR (self, STREAM, FORMAT, (NULL),
        ("codec_data larger than supported by OpenMAX port (%zu > %u)",
            gst_buffer_get_size (codec_data),
            (guint) self->dec_in_port->port_def.nBufferSize));
    return GST_FLOW_ERROR;
  }

component_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("OpenMAX component in error state %s (0x%08x)",
            gst_omx_component_get_last_error_string (self->dec),
            gst_omx_component_get_last_error (self->dec)));
    return GST_FLOW_ERROR;
  }

flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- returning FLUSHING");
    return GST_FLOW_FLUSHING;
  }
reconfigure_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Unable to reconfigure input port"));
    return GST_FLOW_ERROR;
  }
release_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Failed to relase input buffer to component: %s (0x%08x)",
            gst_omx_error_to_string (err), err));
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_omx_audio_dec_open (GstAudioDecoder * decoder)
{
  GstOMXAudioDec *self = GST_OMX_AUDIO_DEC (decoder);
  GstOMXAudioDecClass *klass = GST_OMX_AUDIO_DEC_GET_CLASS (self);
  gint in_port_index, out_port_index;

  GST_DEBUG_OBJECT (self, "opening decoder");

  self->dec =
      gst_omx_component_new (GST_OBJECT_CAST (self), klass->cdata.core_name,
      klass->cdata.component_name, klass->cdata.component_role,
      klass->cdata.hacks);

  self->started = FALSE;

  if (!self->dec)
    return FALSE;

  if (gst_omx_component_get_state (self->dec,
          GST_CLOCK_TIME_NONE) != OMX_StateLoaded)
    return FALSE;

  in_port_index = klass->cdata.in_port_index;
  out_port_index = klass->cdata.out_port_index;

  if (in_port_index == -1 || out_port_index == -1) {
    OMX_PORT_PARAM_TYPE param;
    OMX_ERRORTYPE err;

    GST_OMX_INIT_STRUCT (&param);

    err =
        gst_omx_component_get_parameter (self->dec, OMX_IndexParamAudioInit,
        &param);
    if (err != OMX_ErrorNone) {
      GST_WARNING_OBJECT (self, "Couldn't get port information: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      /* Fallback */
      in_port_index = 0;
      out_port_index = 1;
    } else {
      GST_DEBUG_OBJECT (self, "Detected %u ports, starting at %u",
          (guint) param.nPorts, (guint) param.nStartPortNumber);
      in_port_index = param.nStartPortNumber + 0;
      out_port_index = param.nStartPortNumber + 1;
    }
  }

  self->dec_in_port = gst_omx_component_add_port (self->dec, in_port_index);
  self->dec_out_port = gst_omx_component_add_port (self->dec, out_port_index);

  if (!self->dec_in_port || !self->dec_out_port)
    return FALSE;

  GST_DEBUG_OBJECT (self, "Opened decoder");

  return TRUE;
}

static gboolean
gst_omx_audio_dec_shutdown (GstOMXAudioDec * self)
{
  OMX_STATETYPE state;

  GST_DEBUG_OBJECT (self, "Shutting down decoder");

  state = gst_omx_component_get_state (self->dec, 0);
  if (state > OMX_StateLoaded || state == OMX_StateInvalid) {
    if (state > OMX_StateIdle) {
      gst_omx_component_set_state (self->dec, OMX_StateIdle);
      gst_omx_component_get_state (self->dec, 5 * GST_SECOND);
    }
    gst_omx_component_set_state (self->dec, OMX_StateLoaded);
    gst_omx_port_deallocate_buffers (self->dec_in_port);
    gst_omx_port_deallocate_buffers (self->dec_out_port);

    if (state > OMX_StateLoaded)
      gst_omx_component_get_state (self->dec, 5 * GST_SECOND);
  }

  return TRUE;
}

static gboolean
gst_omx_audio_dec_close (GstAudioDecoder * decoder)
{
  GstOMXAudioDec *self = GST_OMX_AUDIO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "closing decoder");

  if (!gst_omx_audio_dec_shutdown (self))
    return FALSE;

  self->dec_in_port = NULL;
  self->dec_out_port = NULL;
  if (self->dec)
    gst_omx_component_free (self->dec);
  self->dec = NULL;


  self->started = FALSE;

  GST_DEBUG_OBJECT (self, "Closed decoder");

  return TRUE;
}
