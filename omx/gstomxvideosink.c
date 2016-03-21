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
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#include "gstomxvideosink.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_video_sink_debug_category);
#define GST_CAT_DEFAULT gst_omx_video_sink_debug_category

#define GST_OMX_SINK_MEMORY_TYPE "omxsink"

/* OpenMax memory allocator Implementation */

typedef struct _GstOmxSinkMemory GstOmxSinkMemory;
typedef struct _GstOmxSinkMemoryAllocator GstOmxSinkMemoryAllocator;
typedef struct _GstOmxSinkMemoryAllocatorClass GstOmxSinkMemoryAllocatorClass;

struct _GstOmxSinkMemory
{
  GstMemory mem;

  GstOMXBuffer *buf;
};

struct _GstOmxSinkMemoryAllocator
{
  GstAllocator parent;
};

struct _GstOmxSinkMemoryAllocatorClass
{
  GstAllocatorClass parent_class;
};

static GstMemory *
gst_omx_sink_memory_allocator_alloc_dummy (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_assert_not_reached ();
  return NULL;
}

static void
gst_omx_sink_memory_allocator_free (GstAllocator * allocator, GstMemory * mem)
{
  GstOmxSinkMemory *omem = (GstOmxSinkMemory *) mem;
  g_slice_free (GstOmxSinkMemory, omem);
}

static gpointer
gst_omx_sink_memory_map (GstMemory * mem, gsize maxsize, GstMapFlags flags)
{
  GstOmxSinkMemory *omem = (GstOmxSinkMemory *) mem;
  return omem->buf->omx_buf->pBuffer + omem->mem.offset;
}

static void
gst_omx_sink_memory_unmap (GstMemory * mem)
{
}

static GstMemory *
gst_omx_sink_memory_share (GstMemory * mem, gssize offset, gssize size)
{
  g_assert_not_reached ();
  return NULL;
}

GType gst_omx_sink_memory_allocator_get_type (void);
G_DEFINE_TYPE (GstOmxSinkMemoryAllocator, gst_omx_sink_memory_allocator,
    GST_TYPE_ALLOCATOR);

#define GST_TYPE_OMX_SINK_MEMORY_ALLOCATOR   (gst_omx_sink_memory_allocator_get_type())
#define GST_IS_OMX_SINK_MEMORY_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_OMX_SINK_MEMORY_ALLOCATOR))

#define DEFAULT_OVERLAY        1
#define DEFAULT_OVERLAY_DEPTH  0
#define DEFAULT_OVERLAY_X      0
#define DEFAULT_OVERLAY_Y      0
#define DEFAULT_OVERLAY_W      0
#define DEFAULT_OVERLAY_H      0

enum
{
  PROP_0,
  PROP_OVERLAY,
  PROP_OVERLAY_DEPTH,
  PROP_OVERLAY_X,
  PROP_OVERLAY_Y,
  PROP_OVERLAY_W,
  PROP_OVERLAY_H
};

static void
gst_omx_sink_memory_allocator_class_init (GstOmxSinkMemoryAllocatorClass *
    klass)
{
  GstAllocatorClass *allocator_class;

  allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = gst_omx_sink_memory_allocator_alloc_dummy;
  allocator_class->free = gst_omx_sink_memory_allocator_free;
}

static void
gst_omx_sink_memory_allocator_init (GstOmxSinkMemoryAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_OMX_SINK_MEMORY_TYPE;
  alloc->mem_map = gst_omx_sink_memory_map;
  alloc->mem_unmap = gst_omx_sink_memory_unmap;
  alloc->mem_share = gst_omx_sink_memory_share;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static GstMemory *
gst_omx_sink_memory_allocator_alloc (GstAllocator * allocator,
    GstMemoryFlags flags, GstOMXBuffer * buf)
{
  GstOmxSinkMemory *mem;

  mem = g_slice_new (GstOmxSinkMemory);
  /* the shared memory is always readonly */
  gst_memory_init (GST_MEMORY_CAST (mem), flags, allocator, NULL,
      buf->omx_buf->nAllocLen, buf->port->port_def.nBufferAlignment,
      0, buf->omx_buf->nAllocLen);

  mem->buf = buf;

  return GST_MEMORY_CAST (mem);
}

/* Buffer Pool of openmax buffers */

GQuark gst_omx_sink_data_quark = 0;
typedef struct _GstOmxSinkBufferPool GstOmxSinkBufferPool;
typedef struct _GstOmxSinkBufferPoolClass GstOmxSinkBufferPoolClass;
#define GST_OMX_SINK_BUFFER_POOL(pool)  ((GstOmxSinkBufferPool *) pool)

struct _GstOmxSinkBufferPool
{
  GstBufferPool parent;

  GstElement *element;

  GstCaps *caps;
  gboolean add_videometa;
  GstVideoInfo video_info;

  GstOMXComponent *component;
  GstOMXPort *port;

  GstAllocator *allocator;

  /* Used during alloc to specify
   * which buffer has to be wrapped.
   */
  guint current_buffer_index;
};

struct _GstOmxSinkBufferPoolClass
{
  GstBufferPoolClass parent_class;
};

GType gst_omx_sink_buffer_pool_get_type (void);

G_DEFINE_TYPE (GstOmxSinkBufferPool, gst_omx_sink_buffer_pool,
    GST_TYPE_BUFFER_POOL);

#define GST_TYPE_OMX_SINK_BUFFER_POOL   (gst_omx_sink_buffer_pool_get_type())


static void
gst_omx_sink_buffer_pool_finalize (GObject * object)
{
  GstOmxSinkBufferPool *pool = GST_OMX_SINK_BUFFER_POOL (object);

  if (pool->element)
    gst_object_unref (pool->element);
  pool->element = NULL;

  if (pool->allocator)
    gst_object_unref (pool->allocator);
  pool->allocator = NULL;

  if (pool->caps)
    gst_caps_unref (pool->caps);
  pool->caps = NULL;

  G_OBJECT_CLASS (gst_omx_sink_buffer_pool_parent_class)->finalize (object);
}

static gboolean
gst_omx_sink_buffer_pool_start (GstBufferPool * bpool)
{
  GstOmxSinkBufferPool *pool = GST_OMX_SINK_BUFFER_POOL (bpool);
  GstOmxVideoSink *self = GST_OMX_VIDEO_SINK (pool->element);
  GstCaps *caps;
  guint min, max;
  GstStructure *config;
  GstOMXPort *port = pool->port;
  OMX_ERRORTYPE err = OMX_ErrorNone;


  /* Only allow to start the pool if we still are attached
   * to a component and port */
  GST_OBJECT_LOCK (pool);
  if (!pool->component || !pool->port) {
    GST_OBJECT_UNLOCK (pool);
    return FALSE;
  }

  config = gst_buffer_pool_get_config (bpool);
  gst_buffer_pool_config_get_params (config, &caps, NULL, &min, &max);
  gst_structure_free (config);

  min = MAX (min, max);

  if (min > port->port_def.nBufferCountActual) {
    err = gst_omx_port_update_port_definition (port, NULL);
    if (err == OMX_ErrorNone) {
      port->port_def.nBufferCountActual = min;
      err = gst_omx_port_update_port_definition (port, &port->port_def);
    }
  }
  if (!gst_omx_port_is_enabled (port)) {
    err = gst_omx_port_set_enabled (port, TRUE);
    if (err != OMX_ErrorNone) {
      GST_INFO_OBJECT (self,
          "Failed to enable port: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      GST_OBJECT_UNLOCK (pool);
      return FALSE;
    }
    if (gst_omx_port_allocate_buffers (self->sink_in_port) != OMX_ErrorNone) {
      GST_OBJECT_UNLOCK (pool);
      return FALSE;
    }
    if (gst_omx_port_wait_enabled (self->sink_in_port,
            5 * GST_SECOND) != OMX_ErrorNone) {
      GST_OBJECT_UNLOCK (pool);
      return FALSE;
    }
  }

  GST_OBJECT_UNLOCK (pool);

  return
      GST_BUFFER_POOL_CLASS (gst_omx_sink_buffer_pool_parent_class)->start
      (bpool);
}

static gboolean
gst_omx_sink_buffer_pool_stop (GstBufferPool * bpool)
{
  GstOmxSinkBufferPool *pool = GST_OMX_SINK_BUFFER_POOL (bpool);

  if (pool->caps)
    gst_caps_unref (pool->caps);
  pool->caps = NULL;

  pool->add_videometa = FALSE;

  return
      GST_BUFFER_POOL_CLASS (gst_omx_sink_buffer_pool_parent_class)->stop
      (bpool);
}

static const gchar **
gst_omx_sink_buffer_pool_get_options (GstBufferPool * bpool)
{
  static const gchar *raw_video_options[] =
      { GST_BUFFER_POOL_OPTION_VIDEO_META, NULL };
  static const gchar *options[] = { NULL };
  GstOmxSinkBufferPool *pool = GST_OMX_SINK_BUFFER_POOL (bpool);

  GST_OBJECT_LOCK (pool);
  if (pool->port && pool->port->port_def.eDomain == OMX_PortDomainVideo
      && pool->port->port_def.format.video.eCompressionFormat ==
      OMX_VIDEO_CodingUnused) {
    GST_OBJECT_UNLOCK (pool);
    return raw_video_options;
  }
  GST_OBJECT_UNLOCK (pool);

  return options;
}

static gboolean
gst_omx_sink_buffer_pool_set_config (GstBufferPool * bpool,
    GstStructure * config)
{
  GstOmxSinkBufferPool *pool = GST_OMX_SINK_BUFFER_POOL (bpool);
  GstCaps *caps;

  GST_OBJECT_LOCK (pool);

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL))
    goto wrong_config;

  if (caps == NULL)
    goto no_caps;

  if (pool->port && pool->port->port_def.eDomain == OMX_PortDomainVideo
      && pool->port->port_def.format.video.eCompressionFormat ==
      OMX_VIDEO_CodingUnused) {
    GstVideoInfo info;

    /* now parse the caps from the config */
    if (!gst_video_info_from_caps (&info, caps))
      goto wrong_video_caps;

    /* enable metadata based on config of the pool */
    pool->add_videometa =
        gst_buffer_pool_config_has_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    pool->video_info = info;
  }

  if (pool->caps)
    gst_caps_unref (pool->caps);
  pool->caps = gst_caps_ref (caps);

  GST_OBJECT_UNLOCK (pool);

  return
      GST_BUFFER_POOL_CLASS (gst_omx_sink_buffer_pool_parent_class)->set_config
      (bpool, config);

  /* ERRORS */
wrong_config:
  {
    GST_OBJECT_UNLOCK (pool);
    GST_WARNING_OBJECT (pool, "invalid config");
    return FALSE;
  }
no_caps:
  {
    GST_OBJECT_UNLOCK (pool);
    GST_WARNING_OBJECT (pool, "no caps in config");
    return FALSE;
  }
wrong_video_caps:
  {
    GST_OBJECT_UNLOCK (pool);
    GST_WARNING_OBJECT (pool,
        "failed getting geometry from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
}

static GstFlowReturn
gst_omx_sink_buffer_pool_alloc_buffer (GstBufferPool * bpool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstOmxSinkBufferPool *pool = GST_OMX_SINK_BUFFER_POOL (bpool);
  GstBuffer *buf;
  GstOMXBuffer *omx_buf;
  GstMemory *mem;

  omx_buf = g_ptr_array_index (pool->port->buffers, pool->current_buffer_index);
  g_return_val_if_fail (omx_buf != NULL, GST_FLOW_ERROR);

  mem = gst_omx_sink_memory_allocator_alloc (pool->allocator, 0, omx_buf);
  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf, mem);

  if (pool->add_videometa) {
    gsize offset[4] = { 0, };
    gint stride[4] = { 0, };

    switch (pool->video_info.finfo->format) {
      case GST_VIDEO_FORMAT_I420:
        offset[0] = 0;
        stride[0] = pool->port->port_def.format.video.nStride;
        offset[1] = stride[0] * pool->port->port_def.format.video.nSliceHeight;
        stride[1] = pool->port->port_def.format.video.nStride / 2;
        offset[2] =
            offset[1] +
            stride[1] * (pool->port->port_def.format.video.nSliceHeight / 2);
        stride[2] = pool->port->port_def.format.video.nStride / 2;
        break;
      case GST_VIDEO_FORMAT_NV12:
        offset[0] = 0;
        stride[0] = pool->port->port_def.format.video.nStride;
        offset[1] = stride[0] * pool->port->port_def.format.video.nSliceHeight;
        stride[1] = pool->port->port_def.format.video.nStride;
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (&pool->video_info),
        GST_VIDEO_INFO_WIDTH (&pool->video_info),
        GST_VIDEO_INFO_HEIGHT (&pool->video_info),
        GST_VIDEO_INFO_N_PLANES (&pool->video_info), offset, stride);
  }

  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (buf),
      gst_omx_sink_data_quark, omx_buf, NULL);

  *buffer = buf;

  pool->current_buffer_index++;

  return GST_FLOW_OK;
}

static void
gst_omx_sink_buffer_pool_free_buffer (GstBufferPool * bpool, GstBuffer * buffer)
{
  GstOmxSinkBufferPool *pool = GST_OMX_SINK_BUFFER_POOL (bpool);

  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (buffer),
      gst_omx_sink_data_quark, NULL, NULL);

  GST_BUFFER_POOL_CLASS (gst_omx_sink_buffer_pool_parent_class)->free_buffer
      (bpool, buffer);

  pool->current_buffer_index--;
}

static void
gst_omx_sink_buffer_pool_class_init (GstOmxSinkBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gst_omx_sink_data_quark = g_quark_from_static_string ("GstOmxSinkBufferData");

  gobject_class->finalize = gst_omx_sink_buffer_pool_finalize;
  gstbufferpool_class->start = gst_omx_sink_buffer_pool_start;
  gstbufferpool_class->stop = gst_omx_sink_buffer_pool_stop;
  gstbufferpool_class->get_options = gst_omx_sink_buffer_pool_get_options;
  gstbufferpool_class->set_config = gst_omx_sink_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = gst_omx_sink_buffer_pool_alloc_buffer;
  gstbufferpool_class->free_buffer = gst_omx_sink_buffer_pool_free_buffer;
}

static void
gst_omx_sink_buffer_pool_init (GstOmxSinkBufferPool * pool)
{
  pool->allocator =
      g_object_new (gst_omx_sink_memory_allocator_get_type (), NULL);
  pool->current_buffer_index = 0;
}

static GstBufferPool *
gst_omx_sink_buffer_pool_new (GstElement * element, GstOMXComponent * component,
    GstOMXPort * port)
{
  GstOmxSinkBufferPool *pool;

  pool = g_object_new (GST_TYPE_OMX_SINK_BUFFER_POOL, NULL);
  pool->element = gst_object_ref (element);
  pool->component = component;
  pool->port = port;

  return GST_BUFFER_POOL (pool);
}

/* prototypes */

static GstFlowReturn
gst_omx_video_sink_show_frame (GstVideoSink * video_sink, GstBuffer * buf);

static void
gst_omx_video_sink_check_nvfeatures (GstOmxVideoSink * self, GstCaps * caps);

gboolean gst_omx_video_sink_start (GstBaseSink * sink);
gboolean gst_omx_video_sink_stop (GstBaseSink * videosink);
gboolean gst_omx_video_sink_setcaps (GstBaseSink * sink, GstCaps * caps);
GstCaps *gst_omx_video_sink_getcaps (GstBaseSink * sink, GstCaps * filter);
gboolean gst_omx_video_sink_propose_allocation (GstBaseSink * sink,
    GstQuery * query);
void gst_omx_video_sink_get_times (GstBaseSink * sink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_omx_video_sink_shutdown (GstOmxVideoSink * self);
static gboolean
gst_omx_video_sink_event (GstBaseSink * sink, GstEvent * event);

static void
gst_omx_video_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void
gst_omx_video_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* class initialization */

#define gst_omx_video_sink_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE (GstOmxVideoSink, gst_omx_video_sink,
    GST_TYPE_VIDEO_SINK,
    GST_DEBUG_CATEGORY_INIT (gst_omx_video_sink_debug_category, "omxvideosink",
        0, "debug category for omxvideosink element"));


static OMX_ERRORTYPE
Update_Overlay_PlaneBlend (GstOmxVideoSink * self)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_CONFIG_PLANEBLENDTYPE odepth;

  GST_OMX_INIT_STRUCT (&odepth);
  odepth.nPortIndex = self->sink_in_port->index;

  eError =
      gst_omx_component_get_config (GST_OMX_VIDEO_SINK (self)->sink,
         OMX_IndexConfigCommonPlaneBlend,
         &odepth);
  if (eError == OMX_ErrorNone) {
    odepth.nDepth = self->overlay_depth;
    eError =
        gst_omx_component_set_config (GST_OMX_VIDEO_SINK (self)->sink,
           OMX_IndexConfigCommonPlaneBlend,
           &odepth);
  }

  return eError;
}


static OMX_ERRORTYPE
Update_Overlay_Position (GstOmxVideoSink * self)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_CONFIG_POINTTYPE position;

  if (self->update_pos) {
    GST_OMX_INIT_STRUCT (&position);
    position.nPortIndex = self->sink_in_port->index;

    eError =
        gst_omx_component_get_config (GST_OMX_VIDEO_SINK (self)->sink,
           OMX_IndexConfigCommonOutputPosition,
           &position);
    if (eError == OMX_ErrorNone) {
      position.nX = self->overlay_x;
      position.nY = self->overlay_y;
      eError =
        gst_omx_component_set_config (GST_OMX_VIDEO_SINK (self)->sink,
           OMX_IndexConfigCommonOutputPosition,
           &position);
      self->update_pos = FALSE;
    }
  }

  return eError;
}

static OMX_ERRORTYPE
Update_Overlay_Size (GstOmxVideoSink * self)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_FRAMESIZETYPE osize;

  if (self->update_size) {
    GST_OMX_INIT_STRUCT (&osize);
    osize.nPortIndex = self->sink_in_port->index;

    eError =
        gst_omx_component_get_config (GST_OMX_VIDEO_SINK (self)->sink,
           OMX_IndexConfigCommonOutputSize,
           &osize);
    if (eError == OMX_ErrorNone) {
      osize.nWidth = self->overlay_w;
      osize.nHeight = self->overlay_h;
      eError =
        gst_omx_component_set_config (GST_OMX_VIDEO_SINK (self)->sink,
           OMX_IndexConfigCommonOutputSize,
           &osize);
      self->update_size = FALSE;
    }
  }

  return eError;
}

static GstStateChangeReturn
gst_omx_video_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstOmxVideoSink *omxsink;

  omxsink = GST_OMX_VIDEO_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      omxsink->fps_n = 0;
      omxsink->fps_d = 1;
      GST_VIDEO_SINK_WIDTH (omxsink) = 0;
      GST_VIDEO_SINK_HEIGHT (omxsink) = 0;

      g_mutex_lock (&omxsink->flow_lock);
      if (omxsink->pool)
        gst_buffer_pool_set_active (omxsink->pool, FALSE);
      g_mutex_unlock (&omxsink->flow_lock);

      if (omxsink->sink_in_port)
        gst_omx_port_set_flushing (omxsink->sink_in_port, 5 * GST_SECOND, TRUE);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      /*
       * When running a stream in loop, the pipeline is set in READY state.
       * In this state we need to ensure that buffers on input port of
       * sink are deallocated without the sink going in NULL state.
       * reconfigure flag is set here which is checked in setcaps function
       * to reconfigure sink's input port incase setcaps is called again.
       */
      omxsink->sink_in_port->reconfigure = TRUE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_omx_video_sink_class_init (GstOmxVideoSinkClass * klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstVideoSinkClass *videosink_class = GST_VIDEO_SINK_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gstelement_class->change_state = gst_omx_video_sink_change_state;

  gobject_class->set_property = gst_omx_video_sink_set_property;
  gobject_class->get_property = gst_omx_video_sink_get_property;

  basesink_class->start = GST_DEBUG_FUNCPTR (gst_omx_video_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_omx_video_sink_stop);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_omx_video_sink_setcaps);
  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_video_sink_getcaps);
  basesink_class->get_times = GST_DEBUG_FUNCPTR (gst_omx_video_sink_get_times);
  basesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_omx_video_sink_propose_allocation);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_omx_video_sink_event);

  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_omx_video_sink_show_frame);

  /*
   *  Overlay Index.
   */
  g_object_class_install_property (gobject_class, PROP_OVERLAY,
      g_param_spec_uint ("overlay", "overlay",
          "Overlay index", 1, 2,
          DEFAULT_OVERLAY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /*
   *  Overlay Depth.
   */
  g_object_class_install_property (gobject_class, PROP_OVERLAY_DEPTH,
      g_param_spec_uint ("overlay-depth", "overlay-depth",
          "Overlay depth", 0, 2,
          DEFAULT_OVERLAY_DEPTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /*
   *  Overlay X coordinate.
   */
  g_object_class_install_property (gobject_class, PROP_OVERLAY_X,
      g_param_spec_uint ("overlay-x", "overlay-x",
          "Overlay X coordinate", 0, G_MAXUINT,
          DEFAULT_OVERLAY_X, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /*
   *  Overlay Y coordinate.
   */
  g_object_class_install_property (gobject_class, PROP_OVERLAY_Y,
      g_param_spec_uint ("overlay-y", "overlay-y",
          "Overlay Y coordinate", 0, G_MAXUINT,
          DEFAULT_OVERLAY_Y, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /*
   *  Overlay Width.
   */
  g_object_class_install_property (gobject_class, PROP_OVERLAY_W,
      g_param_spec_uint ("overlay-w", "overlay-w",
          "Overlay Width", 0, G_MAXUINT,
          DEFAULT_OVERLAY_W, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /*
   *  Overlay Height.
   */
  g_object_class_install_property (gobject_class, PROP_OVERLAY_H,
      g_param_spec_uint ("overlay-h", "overlay-h",
          "Overlay Height", 0, G_MAXUINT,
          DEFAULT_OVERLAY_H, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_omx_video_sink_init (GstOmxVideoSink * omxvideosink)
{
  omxvideosink->fps_n = 0;
  omxvideosink->fps_d = 1;
  omxvideosink->cur_buf = NULL;
  omxvideosink->hw_path = FALSE;

  omxvideosink->overlay = DEFAULT_OVERLAY;
  omxvideosink->overlay_depth = DEFAULT_OVERLAY_DEPTH;
  omxvideosink->overlay_x = DEFAULT_OVERLAY_X;
  omxvideosink->overlay_y = DEFAULT_OVERLAY_Y;
  omxvideosink->overlay_w = DEFAULT_OVERLAY_W;
  omxvideosink->overlay_h = DEFAULT_OVERLAY_H;

  omxvideosink->update_pos = FALSE;
  omxvideosink->update_size = FALSE;
}

static void
gst_omx_video_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxVideoSink * self = GST_OMX_VIDEO_SINK (object);

  switch (prop_id) {
    case PROP_OVERLAY:
      self->overlay = g_value_get_uint (value);
      break;
    case PROP_OVERLAY_DEPTH:
      self->overlay_depth = g_value_get_uint (value);
      break;
    case PROP_OVERLAY_X:
    {
      guint x = g_value_get_uint (value);
      if (self->overlay_x != x)
        self->update_pos = TRUE;
      self->overlay_x = x;
    }
      break;
    case PROP_OVERLAY_Y:
    {
      guint y = g_value_get_uint (value);
      if (self->overlay_y != y)
        self->update_pos = TRUE;
      self->overlay_y = y;
    }
      break;
    case PROP_OVERLAY_W:
    {
      guint w = g_value_get_uint (value);
      if (self->overlay_w != w)
        self->update_size = TRUE;
      self->overlay_w = w;
    }
      break;
    case PROP_OVERLAY_H:
    {
      guint h = g_value_get_uint (value);
      if (self->overlay_h != h)
        self->update_size = TRUE;
      self->overlay_h = h;
    }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_video_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxVideoSink * self = GST_OMX_VIDEO_SINK (object);

  switch (prop_id) {
    case PROP_OVERLAY:
      g_value_set_uint (value, self->overlay);
      break;
    case PROP_OVERLAY_DEPTH:
      g_value_set_uint (value, self->overlay_depth);
      break;
    case PROP_OVERLAY_X:
      g_value_set_uint (value, self->overlay_x);
      break;
    case PROP_OVERLAY_Y:
      g_value_set_uint (value, self->overlay_y);
      break;
    case PROP_OVERLAY_W:
      g_value_set_uint (value, self->overlay_w);
      break;
    case PROP_OVERLAY_H:
      g_value_set_uint (value, self->overlay_h);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_omx_video_sink_start (GstBaseSink * videosink)
{
  GstOmxVideoSink *self = GST_OMX_VIDEO_SINK (videosink);
  GstOmxVideoSinkClass *klass = GST_OMX_VIDEO_SINK_GET_CLASS (self);
  gint in_port_index;

  GST_DEBUG_OBJECT (self, "opening video renderer");

  self->sink =
      gst_omx_component_new (GST_OBJECT_CAST (self), klass->cdata.core_name,
      klass->cdata.component_name, klass->cdata.component_role,
      klass->cdata.hacks);

  if (!self->sink)
    return FALSE;

  if (gst_omx_component_get_state (self->sink,
          GST_CLOCK_TIME_NONE) != OMX_StateLoaded)
    return FALSE;

  in_port_index = klass->cdata.in_port_index;

  if (in_port_index == -1) {
    OMX_PORT_PARAM_TYPE param;
    OMX_ERRORTYPE err;

    GST_OMX_INIT_STRUCT (&param);

    err =
        gst_omx_component_get_parameter (self->sink, OMX_IndexParamVideoInit,
        &param);
    if (err != OMX_ErrorNone) {
      GST_WARNING_OBJECT (self, "Couldn't get port information: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      /* Fallback */
      in_port_index = 0;
    } else {
      GST_DEBUG_OBJECT (self, "Detected %lu ports, starting at %lu",
          param.nPorts, param.nStartPortNumber);
      in_port_index = param.nStartPortNumber + 0;
    }
  }
  self->sink_in_port = gst_omx_component_add_port (self->sink, in_port_index);

  if (!self->sink_in_port)
    return FALSE;

  GST_DEBUG_OBJECT (self, "Opened the video renderer");

  return TRUE;
}

static gboolean
gst_omx_video_sink_shutdown (GstOmxVideoSink * self)
{
  OMX_STATETYPE state;

  GST_DEBUG_OBJECT (self, "Shutting down renderer");

  state = gst_omx_component_get_state (self->sink, 0);

  if (state > OMX_StateLoaded || state == OMX_StateInvalid) {
    if (state > OMX_StateIdle) {
      gst_omx_component_set_state (self->sink, OMX_StateIdle);
      gst_omx_component_get_state (self->sink, 5 * GST_SECOND);
    }
    gst_omx_component_set_state (self->sink, OMX_StateLoaded);
    gst_omx_port_deallocate_buffers (self->sink_in_port);

    if (state > OMX_StateLoaded)
      gst_omx_component_get_state (self->sink, 5 * GST_SECOND);
  }

  return TRUE;
}

gboolean
gst_omx_video_sink_stop (GstBaseSink * videosink)
{
  GstOmxVideoSink *self = GST_OMX_VIDEO_SINK (videosink);

  GST_DEBUG_OBJECT (self, "Closing renderer");

  if (!gst_omx_video_sink_shutdown (self))
    return FALSE;

  self->sink_in_port = NULL;
  if (self->sink)
    gst_omx_component_free (self->sink);

  self->sink = NULL;

  if (self->pool)
    gst_object_unref (self->pool);
  self->pool = NULL;

  GST_DEBUG_OBJECT (self, "Closed renderer");

  return TRUE;
}

static void
gst_omx_video_sink_check_nvfeatures (GstOmxVideoSink * self, GstCaps * caps)
{
  GstCapsFeatures *feature;
  feature = gst_caps_get_features (caps, 0);
  if (gst_caps_features_contains (feature, "memory:NVMM")) {
    self->hw_path = TRUE;
  }
}

static OMX_ERRORTYPE
gstomx_use_allow_secondary_window_extension (GstOmxVideoSink * self)
{
  NVX_CONFIG_ALLOWSECONDARYWINDOW param;
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  eError = gst_omx_component_get_index (GST_OMX_VIDEO_SINK (self)->sink,
        (char *) NVX_INDEX_CONFIG_ALLOWSECONDARYWINDOW, &eIndex);
  if (eError == OMX_ErrorNone) {
    GST_OMX_INIT_STRUCT (&param);
    param.nPortIndex = self->sink_in_port->index;
    param.bAllow = OMX_TRUE;
      eError =
        gst_omx_component_set_config (GST_OMX_VIDEO_SINK (self)->sink,
           eIndex,
           &param);
  }

  if (eError != OMX_ErrorNone) {
    g_error ("Couldnt use the Vendor Extension %s \n",
        (char *) NVX_INDEX_CONFIG_ALLOWSECONDARYWINDOW);
  }

  return eError;
}

static OMX_ERRORTYPE
gstomx_use_overlay_index_extension (GstOmxVideoSink * self)
{
  NVX_CONFIG_OVERLAYINDEX param;
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE eError = OMX_ErrorNone;

  eError = gst_omx_component_get_index (GST_OMX_VIDEO_SINK (self)->sink,
        (char *) NVX_INDEX_CONFIG_OVERLAYINDEX, &eIndex);
  if (eError == OMX_ErrorNone) {
    GST_OMX_INIT_STRUCT (&param);
    param.nPortIndex = self->sink_in_port->index;
    param.index = self->overlay;
      eError =
        gst_omx_component_set_config (GST_OMX_VIDEO_SINK (self)->sink,
           eIndex,
           &param);
  }

  if (eError != OMX_ErrorNone) {
    g_error ("Couldnt use the Vendor Extension %s \n",
        (char *) NVX_INDEX_CONFIG_OVERLAYINDEX);
  }

  return eError;
}

gboolean
gst_omx_video_sink_setcaps (GstBaseSink * sink, GstCaps * caps)
{
  GstOmxVideoSink *self = GST_OMX_VIDEO_SINK (sink);
  GstVideoInfo info;
  GstBufferPool *newpool, *oldpool;
  GstStructure *config;
  gint size, min;
  gboolean is_format_change = FALSE;
  gboolean needs_disable = FALSE;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_format;

  GST_DEBUG_OBJECT (self, "Setting new caps %" GST_PTR_FORMAT, caps);

  gst_omx_port_get_port_definition (self->sink_in_port, &port_def);
  gst_omx_video_sink_check_nvfeatures (self, caps);

  /* Check if the caps change is a real format change or if only irrelevant
   * parts of the caps have changed or nothing at all.
   */
  is_format_change |= port_def.format.video.nFrameWidth != info.width;
  is_format_change |= port_def.format.video.nFrameHeight != info.height;

  is_format_change |= (port_def.format.video.xFramerate == 0 && info.fps_n != 0)
      || (port_def.format.video.xFramerate !=
      (info.fps_n << 16) / (info.fps_d));

  needs_disable =
      gst_omx_component_get_state (self->sink,
      GST_CLOCK_TIME_NONE) != OMX_StateLoaded;

  if (needs_disable && !is_format_change && !self->sink_in_port->reconfigure) {
    GST_DEBUG_OBJECT (self,
        "Already running and caps did not change the format");
    return TRUE;
  }

  if (needs_disable && (is_format_change || self->sink_in_port->reconfigure)) {

    GstOMXPort *in_port = self->sink_in_port;

    GST_DEBUG_OBJECT (self, "Need to disable renderer");

    self->sink_in_port->reconfigure = FALSE;
    gst_omx_port_set_flushing (in_port, 5 * GST_SECOND, TRUE);

    if (gst_omx_port_set_enabled (self->sink_in_port, FALSE) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_port_wait_buffers_released (self->sink_in_port,
            5 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_port_deallocate_buffers (self->sink_in_port) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_port_wait_enabled (self->sink_in_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
  }

  GST_VIDEO_SINK_WIDTH (self) = info.width;
  GST_VIDEO_SINK_HEIGHT (self) = info.height;
  self->fps_n = info.fps_n;
  self->fps_d = info.fps_d;

  port_def.format.video.nFrameWidth = info.width;
  port_def.format.video.nFrameHeight = info.height;
  if (info.fps_n == 0)
    port_def.format.video.xFramerate = 0;
  else
    port_def.format.video.xFramerate = (info.fps_n << 16) / (info.fps_d);

  port_def.nBufferSize = info.size;
  size = info.size;
  min = MAX (port_def.nBufferCountMin, 4);
  port_def.nBufferCountActual = min;

  GST_DEBUG_OBJECT (self, "Setting inport port definition");

  if (gst_omx_port_update_port_definition (self->sink_in_port,
          &port_def) != OMX_ErrorNone)
    return FALSE;

  GST_DEBUG_OBJECT (self, "Enabling component");

  if (needs_disable) {
    if (gst_omx_port_mark_reconfigured (self->sink_in_port) != OMX_ErrorNone)
      return FALSE;
  } else {
    if (gst_omx_port_set_enabled (self->sink_in_port, FALSE) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_port_wait_enabled (self->sink_in_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_component_set_state (self->sink,
            OMX_StateIdle) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_component_get_state (self->sink,
            GST_CLOCK_TIME_NONE) != OMX_StateIdle)
      return FALSE;

    if (gst_omx_component_set_state (self->sink,
            OMX_StateExecuting) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_component_get_state (self->sink,
            GST_CLOCK_TIME_NONE) != OMX_StateExecuting)
      return FALSE;
  }

  /* Unset flushing to allow ports to accept data again */
  gst_omx_port_set_flushing (self->sink_in_port, 5 * GST_SECOND, FALSE);

  if (gst_omx_component_get_last_error (self->sink) != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Component in error state: %s (0x%08x)",
        gst_omx_component_get_last_error_string (self->sink),
        gst_omx_component_get_last_error (self->sink));
    return FALSE;
  }

  g_mutex_lock (&self->flow_lock);
  newpool =
      gst_omx_sink_buffer_pool_new (GST_ELEMENT_CAST (self), self->sink,
      self->sink_in_port);

  config = gst_buffer_pool_get_config (newpool);
  gst_buffer_pool_config_set_params (config, caps, size, min, min);
  gst_buffer_pool_config_set_allocator (config,
      ((GstOmxSinkBufferPool *) newpool)->allocator, NULL);
  if (!gst_buffer_pool_set_config (newpool, config))
    goto config_failed;

  oldpool = self->pool;
  self->pool = newpool;
  g_mutex_unlock (&self->flow_lock);

  /* unref the old sink */
  if (oldpool) {
    gst_object_unref (oldpool);
  }

#ifdef USE_OMX_TARGET_TEGRA
  gstomx_use_allow_secondary_window_extension (self);
  gstomx_use_overlay_index_extension (self);
#endif

  if (OMX_ErrorNone != Update_Overlay_PlaneBlend (self))
    GST_ERROR_OBJECT (self, "Failed to set Overlay depth");

  if (OMX_ErrorNone != Update_Overlay_Position (self))
    GST_ERROR_OBJECT (self, "Failed to set Overlay Position");

  if (OMX_ErrorNone != Update_Overlay_Size (self))
    GST_ERROR_OBJECT (self, "Failed to set Overlay Width");

  return TRUE;

config_failed:
  {
    GST_ERROR_OBJECT (self, "failed to set config.");
    g_mutex_unlock (&self->flow_lock);
    return FALSE;
  }
invalid_format:
  {
    GST_ERROR_OBJECT (self, "caps invalid");
    return FALSE;
  }
}

GstCaps *
gst_omx_video_sink_getcaps (GstBaseSink * sink, GstCaps * filter)
{
  GstCaps *caps = NULL;
  GstOmxVideoSink *self = GST_OMX_VIDEO_SINK (sink);

  caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (self));

  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  return caps;
}

static GstFlowReturn
gst_omx_video_sink_show_frame (GstVideoSink * video_sink, GstBuffer * buf)
{

  GstOmxVideoSink *self = GST_OMX_VIDEO_SINK (video_sink);
  GstOMXBuffer *omxbuf;
  GstMemory *mem;
  GstFlowReturn res = GST_FLOW_OK;
  GstClockTime bufpts = GST_BUFFER_PTS (buf);
  OMX_ERRORTYPE err = OMX_ErrorNone;

  GST_DEBUG_OBJECT (self, "Received the frame");

  /*
     if (buf && self->cur_buf != buf) {
     if (self->cur_buf) {
     GST_LOG_OBJECT (self, "unreffing %p", self->cur_buf);
     gst_buffer_unref (self->cur_buf);
     }
     GST_LOG_OBJECT (self, "reffing %p as our current buffer", buf);
     self->cur_buf = gst_buffer_ref (buf);
     }
   */

  mem = gst_buffer_peek_memory (buf, 0);

  if (mem
      && g_strcmp0 (mem->allocator->mem_type, GST_OMX_SINK_MEMORY_TYPE) == 0) {
    /* Buffer from our pool, can be directly released without copy */

    omxbuf = gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (buf),
        gst_omx_sink_data_quark);

    gst_omx_handle_messages (self->sink);

    /*
     * The number of buffers in the decoder output pool is maintained
     * using the length of the pending_buffers queue
     */
    g_queue_pop_head (&self->sink_in_port->pending_buffers);

    /*
     * To prevent deadlock when no buffers are available in the
     * decoder output buffer pool, we make sure there is at least
     * one buffer in the pool
     */
    while (g_queue_get_length (&self->sink_in_port->pending_buffers) == 0) {
      g_mutex_lock (&self->sink->messages_lock);
      g_cond_wait (&self->sink->messages_cond, &self->sink->messages_lock);
      g_mutex_unlock (&self->sink->messages_lock);
      gst_omx_handle_messages (self->sink);
    }
    res = GST_FLOW_OK;
  } else {
/* Buffer is not from our pool, copy data */

    GstMapInfo map = GST_MAP_INFO_INIT;
    GstOMXPort *port = self->sink_in_port;

    if (!gst_omx_port_is_enabled (port)) {
      err = gst_omx_port_set_enabled (port, TRUE);
      if (err != OMX_ErrorNone) {
        GST_INFO_OBJECT (self,
            "Failed to enable port: %s (0x%08x)",
            gst_omx_error_to_string (err), err);
        res = GST_FLOW_ERROR;
        goto done;
      }

      if (gst_omx_port_allocate_buffers (self->sink_in_port) != OMX_ErrorNone) {
        res = GST_FLOW_ERROR;
        goto done;
      }

      if (gst_omx_port_wait_enabled (self->sink_in_port,
              5 * GST_SECOND) != OMX_ErrorNone) {
        res = GST_FLOW_ERROR;
        goto done;
      }
    }

    if (gst_omx_port_acquire_buffer (self->sink_in_port,
            &omxbuf) != GST_OMX_ACQUIRE_BUFFER_OK) {
      res = GST_FLOW_ERROR;
      goto done;
    }
    gst_buffer_map (buf, &map, GST_MAP_READ);
    memcpy (omxbuf->omx_buf->pBuffer + omxbuf->omx_buf->nOffset,
        map.data, map.size);
    gst_buffer_unmap (buf, &map);
  }
  omxbuf->omx_buf->nFilledLen = mem->size;
  omxbuf->gst_buf = gst_buffer_ref (buf);

  if (self->hw_path)
    omxbuf->omx_buf->nFlags |= OMX_BUFFERFLAG_NV_BUFFER;

  if (GST_CLOCK_TIME_IS_VALID (bufpts))
    omxbuf->omx_buf->nTimeStamp =
        gst_util_uint64_scale (bufpts, OMX_TICKS_PER_SECOND, GST_SECOND);
  else
    omxbuf->omx_buf->nTimeStamp = 0;

  gst_omx_port_release_buffer (self->sink_in_port, omxbuf);

done:

  return res;
}

gboolean
gst_omx_video_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstOmxVideoSink *omxsink = GST_OMX_VIDEO_SINK (bsink);
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  guint size;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  g_mutex_lock (&omxsink->flow_lock);
  if ((pool = omxsink->pool))
    gst_object_ref (pool);
  g_mutex_unlock (&omxsink->flow_lock);

  if (pool != NULL) {
    GstCaps *pcaps;

    /* we had a pool, check caps */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    GST_DEBUG_OBJECT (omxsink,
        "we had a pool with caps %" GST_PTR_FORMAT, pcaps);
    if (!gst_caps_is_equal (caps, pcaps)) {
      /* different caps, we can't use this pool */
      GST_DEBUG_OBJECT (omxsink, "pool has different caps");
      gst_object_unref (pool);
      pool = NULL;
    }
    gst_structure_free (config);
  }
  if (pool == NULL && need_pool) {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    GST_DEBUG_OBJECT (omxsink, "create new pool");
    pool =
        gst_omx_sink_buffer_pool_new (GST_ELEMENT_CAST (omxsink), omxsink->sink,
        omxsink->sink_in_port);

    /* the normal size of a frame */
    size = info.size;

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 2, 4);
    gst_buffer_pool_config_set_allocator (config,
        ((GstOmxSinkBufferPool *) pool)->allocator, NULL);
    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }
  if (pool) {
    gst_query_add_allocation_pool (query, pool, size, 2, 4);
    gst_query_add_allocation_param (query,
        ((GstOmxSinkBufferPool *) pool)->allocator, NULL);
    gst_object_unref (pool);
  }
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
no_caps:
  {
    GST_DEBUG_OBJECT (bsink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (bsink, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (bsink, "failed setting config");
    gst_object_unref (pool);
    return FALSE;
  }
}

void
gst_omx_video_sink_get_times (GstBaseSink * sink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstOmxVideoSink *self;

  self = GST_OMX_VIDEO_SINK (sink);

  if (GST_BUFFER_PTS_IS_VALID (buffer)) {
    *start = GST_BUFFER_PTS (buffer);
    if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
      *end = *start + GST_BUFFER_DURATION (buffer);
    } else {
      if (self->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, self->fps_d, self->fps_n);
      }
    }
  }
}

static gboolean
gst_omx_video_sink_event (GstBaseSink * sink, GstEvent * event)
{

  GstOmxVideoSink *omxsink = GST_OMX_VIDEO_SINK (sink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM: {
      if (gst_event_has_name (event, "ReleaseLastBuffer")) {
        GstOMXBuffer *omxbuf;
        gst_omx_port_acquire_buffer (omxsink->sink_in_port, &omxbuf);
        omxbuf->gst_buf = NULL;

        if (G_LIKELY (omxbuf)) {
            omxbuf->omx_buf->nFilledLen = 0;
            omxbuf->omx_buf->nFlags |= OMX_BUFFERFLAG_EOS;
            gst_omx_port_release_buffer (omxsink->sink_in_port, omxbuf);
        }
        if (gst_base_sink_is_last_sample_enabled (sink)) {
          gst_base_sink_set_last_sample_enabled(sink, FALSE);
          gst_base_sink_set_last_sample_enabled(sink, TRUE);
        }
        gst_omx_port_wait_buffers_released (omxsink->sink_in_port,
            5 * GST_SECOND);
      }
      break;
    }
    default:
      break;
  }
  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
}
