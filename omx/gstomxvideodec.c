/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 * Copyright (C) 2013, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (c) 2013 - 2015, NVIDIA CORPORATION.  All rights reserved.
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
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>
#include <gst/video/gstvideoaffinetransformationmeta.h>

#if defined (USE_OMX_TARGET_RPI) && defined(__GNUC__)
#ifndef __VCCOREVER__
#define __VCCOREVER__ 0x04000000
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC optimize ("gnu89-inline")
#endif

#if (defined (USE_OMX_TARGET_RPI) || defined (USE_OMX_TARGET_TEGRA)) && defined (HAVE_GST_GL)
#include <gst/gl/gl.h>
#include <gst/gl/egl/gstglmemoryegl.h>
#include <gst/gl/egl/gstglcontext_egl.h>
#endif

#if defined (USE_OMX_TARGET_RPI) && defined(__GNUC__)
#pragma GCC reset_options
#pragma GCC diagnostic pop
#endif

#include <string.h>

#include "gstomxvideodec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_video_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_video_dec_debug_category

#ifdef USE_OMX_TARGET_TEGRA
#define DEFAULT_USE_OMXDEC_RES      FALSE
#endif

typedef struct _GstOMXMemory GstOMXMemory;
typedef struct _GstOMXMemoryAllocator GstOMXMemoryAllocator;
typedef struct _GstOMXMemoryAllocatorClass GstOMXMemoryAllocatorClass;

struct _GstOMXMemory
{
  GstMemory mem;

  GstOMXBuffer *buf;
};

struct _GstOMXMemoryAllocator
{
  GstAllocator parent;
};

struct _GstOMXMemoryAllocatorClass
{
  GstAllocatorClass parent_class;
};

#define GST_OMX_MEMORY_TYPE "openmax"
#define GST_OMX_SINK_MEMORY_TYPE "omxsink"

#define DEFAULT_SKIP_FRAME_TYPE GST_DECODE_ALL
#define GST_TYPE_OMX_VID_DEC_SKIP_FRAMES (gst_video_dec_skip_frames ())
static GType
gst_video_dec_skip_frames (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {GST_DECODE_ALL, "GST_OMX_DECODE_ALL_FRAMES", "DECODE_ALL_Frames"},
      {GST_SKIP_NON_REF_FRAMES, "GST_OMX_DECODE_SKIP_NON_REF_FRAMES",
          "SKIP_NON_REF_FRAMES"},
      {GST_DECODE_KEY_FRAMES, "GST_OMX_DECODE_KEY_FRAMES", "DECODE_KEY_FRAMES"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("H264SkipFrame", values);
  }
  return qtype;
}

static GstMemory *
gst_omx_memory_allocator_alloc_dummy (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_assert_not_reached ();
  return NULL;
}

static void
gst_omx_memory_allocator_free (GstAllocator * allocator, GstMemory * mem)
{
  GstOMXMemory *omem = (GstOMXMemory *) mem;

  /* TODO: We need to remember which memories are still used
   * so we can wait until everything is released before allocating
   * new memory
   */

  g_slice_free (GstOMXMemory, omem);
}

static gpointer
gst_omx_memory_map (GstMemory * mem, gsize maxsize, GstMapFlags flags)
{
  GstOMXMemory *omem = (GstOMXMemory *) mem;

  return omem->buf->omx_buf->pBuffer + omem->mem.offset;
}

static void
gst_omx_memory_unmap (GstMemory * mem)
{
}

static GstMemory *
gst_omx_memory_share (GstMemory * mem, gssize offset, gssize size)
{
  g_assert_not_reached ();
  return NULL;
}

GType gst_omx_memory_allocator_get_type (void);
G_DEFINE_TYPE (GstOMXMemoryAllocator, gst_omx_memory_allocator,
    GST_TYPE_ALLOCATOR);

#define GST_TYPE_OMX_MEMORY_ALLOCATOR   (gst_omx_memory_allocator_get_type())
#define GST_IS_OMX_MEMORY_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_OMX_MEMORY_ALLOCATOR))

static void
gst_omx_memory_allocator_class_init (GstOMXMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class;

  allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = gst_omx_memory_allocator_alloc_dummy;
  allocator_class->free = gst_omx_memory_allocator_free;
}

static void
gst_omx_memory_allocator_init (GstOMXMemoryAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_OMX_MEMORY_TYPE;
  alloc->mem_map = gst_omx_memory_map;
  alloc->mem_unmap = gst_omx_memory_unmap;
  alloc->mem_share = gst_omx_memory_share;

  /* default copy & is_span */

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static GstMemory *
gst_omx_memory_allocator_alloc (GstAllocator * allocator, GstMemoryFlags flags,
    GstOMXBuffer * buf)
{
  GstOMXMemory *mem;

  /* FIXME: We don't allow sharing because we need to know
   * when the memory becomes unused and can only then put
   * it back to the pool. Which is done in the pool's release
   * function
   */
  flags |= GST_MEMORY_FLAG_NO_SHARE;

  mem = g_slice_new (GstOMXMemory);
  /* the shared memory is always readonly */
  gst_memory_init (GST_MEMORY_CAST (mem), flags, allocator, NULL,
      buf->omx_buf->nAllocLen, buf->port->port_def.nBufferAlignment,
      0, buf->omx_buf->nAllocLen);

  mem->buf = buf;

  return GST_MEMORY_CAST (mem);
}

/* Buffer pool for the buffers of an OpenMAX port.
 *
 * This pool is only used if we either passed buffers from another
 * pool to the OMX port or provide the OMX buffers directly to other
 * elements.
 *
 *
 * A buffer is in the pool if it is currently owned by the port,
 * i.e. after OMX_{Fill,Empty}ThisBuffer(). A buffer is outside
 * the pool after it was taken from the port after it was handled
 * by the port, i.e. {Empty,Fill}BufferDone.
 *
 * Buffers can be allocated by us (OMX_AllocateBuffer()) or allocated
 * by someone else and (temporarily) passed to this pool
 * (OMX_UseBuffer(), OMX_UseEGLImage()). In the latter case the pool of
 * the buffer will be overriden, and restored in free_buffer(). Other
 * buffers are just freed there.
 *
 * The pool always has a fixed number of minimum and maximum buffers
 * and these are allocated while starting the pool and released afterwards.
 * They correspond 1:1 to the OMX buffers of the port, which are allocated
 * before the pool is started.
 *
 * Acquiring a buffer from this pool happens after the OMX buffer has
 * been acquired from the port. gst_buffer_pool_acquire_buffer() is
 * supposed to return the buffer that corresponds to the OMX buffer.
 *
 * For buffers provided to upstream, the buffer will be passed to
 * the component manually when it arrives and then unreffed. If the
 * buffer is released before reaching the component it will be just put
 * back into the pool as if EmptyBufferDone has happened. If it was
 * passed to the component, it will be back into the pool when it was
 * released and EmptyBufferDone has happened.
 *
 * For buffers provided to downstream, the buffer will be returned
 * back to the component (OMX_FillThisBuffer()) when it is released.
 */

static GQuark gst_omx_buffer_data_quark = 0;
extern GQuark gst_omx_sink_data_quark;

#define GST_OMX_BUFFER_POOL(pool) ((GstOMXBufferPool *) pool)
typedef struct _GstOMXBufferPool GstOMXBufferPool;
typedef struct _GstOMXBufferPoolClass GstOMXBufferPoolClass;

struct _GstOMXBufferPool
{
  GstVideoBufferPool parent;

  GstElement *element;

  GstCaps *caps;
  gboolean add_videometa;
  GstVideoInfo video_info;

  /* Owned by element, element has to stop this pool before
   * it destroys component or port */
  GstOMXComponent *component;
  GstOMXPort *port;

  /* For handling OpenMAX allocated memory */
  GstAllocator *allocator;

  /* Set from outside this pool */
  /* TRUE if we're currently allocating all our buffers */
  gboolean allocating;

  /* TRUE if the pool is not used anymore */
  gboolean deactivated;

  /* For populating the pool from another one */
  GstBufferPool *other_pool;
  GPtrArray *buffers;

  /* Used during acquire for output ports to
   * specify which buffer has to be retrieved
   * and during alloc, which buffer has to be
   * wrapped
   */
  gint current_buffer_index;
};

struct _GstOMXBufferPoolClass
{
  GstVideoBufferPoolClass parent_class;
};

GType gst_omx_buffer_pool_get_type (void);

G_DEFINE_TYPE (GstOMXBufferPool, gst_omx_buffer_pool, GST_TYPE_BUFFER_POOL);

static void
gst_omx_buffer_pool_free_buffer (GstBufferPool * bpool, GstBuffer * buffer);

static gboolean
gst_omx_buffer_pool_start (GstBufferPool * bpool)
{
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (bpool);

  /* Only allow to start the pool if we still are attached
   * to a component and port */
  GST_OBJECT_LOCK (pool);
  if (!pool->component || !pool->port) {
    GST_OBJECT_UNLOCK (pool);
    return FALSE;
  }
  GST_OBJECT_UNLOCK (pool);

  return
      GST_BUFFER_POOL_CLASS (gst_omx_buffer_pool_parent_class)->start (bpool);
}

static gboolean
gst_omx_buffer_pool_stop (GstBufferPool * bpool)
{
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (bpool);
  GstBuffer *buf;
  guint i;

  /* Remove any buffers that are there */
  if (pool->buffers) {
    for (i = 0; i < pool->buffers->len; i++) {
      buf = g_ptr_array_index (pool->buffers, i);
      gst_omx_buffer_pool_free_buffer (bpool, buf);
    }
    g_ptr_array_set_size (pool->buffers, 0);
  }

  if (pool->caps)
    gst_caps_unref (pool->caps);
  pool->caps = NULL;

  pool->add_videometa = FALSE;

  return GST_BUFFER_POOL_CLASS (gst_omx_buffer_pool_parent_class)->stop (bpool);
}

static const gchar **
gst_omx_buffer_pool_get_options (GstBufferPool * bpool)
{
  static const gchar *raw_video_options[] =
      { GST_BUFFER_POOL_OPTION_VIDEO_META, NULL };
  static const gchar *options[] = { NULL };
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (bpool);

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
gst_omx_buffer_pool_set_config (GstBufferPool * bpool, GstStructure * config)
{
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (bpool);
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

  return GST_BUFFER_POOL_CLASS (gst_omx_buffer_pool_parent_class)->set_config
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
gst_omx_buffer_pool_alloc_buffer (GstBufferPool * bpool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (bpool);
  GstBuffer *buf;
  GstOMXBuffer *omx_buf;

  g_return_val_if_fail (pool->allocating, GST_FLOW_ERROR);

  omx_buf = g_ptr_array_index (pool->port->buffers, pool->current_buffer_index);
  g_return_val_if_fail (omx_buf != NULL, GST_FLOW_ERROR);

  if (pool->other_pool) {
    guint i, n;

    buf = g_ptr_array_index (pool->buffers, pool->current_buffer_index);
    g_assert (pool->other_pool == buf->pool);
    gst_object_replace ((GstObject **) & buf->pool, NULL);

    n = gst_buffer_n_memory (buf);
    for (i = 0; i < n; i++) {
      GstMemory *mem = gst_buffer_peek_memory (buf, i);

      /* FIXME: We don't allow sharing because we need to know
       * when the memory becomes unused and can only then put
       * it back to the pool. Which is done in the pool's release
       * function
       */
      GST_MINI_OBJECT_FLAG_SET (mem, GST_MEMORY_FLAG_NO_SHARE);
    }

    if (pool->add_videometa) {
      GstVideoMeta *meta;

      meta = gst_buffer_get_video_meta (buf);
      if (!meta) {
        gst_buffer_add_video_meta (buf, GST_VIDEO_FRAME_FLAG_NONE,
            GST_VIDEO_INFO_FORMAT (&pool->video_info),
            GST_VIDEO_INFO_WIDTH (&pool->video_info),
            GST_VIDEO_INFO_HEIGHT (&pool->video_info));
      }
    }
  } else {
    GstMemory *mem;

    mem = gst_omx_memory_allocator_alloc (pool->allocator, 0, omx_buf);
    buf = gst_buffer_new ();
    gst_buffer_append_memory (buf, mem);
    g_ptr_array_add (pool->buffers, buf);

    if (pool->add_videometa) {
      gsize offset[4] = { 0, };
      gint stride[4] = { 0, };

#ifdef USE_OMX_TARGET_TEGRA
      OMX_INDEXTYPE eIndex;
      GstOMXVideoDec *self = (GstOMXVideoDec *) pool->element;
      OMX_ERRORTYPE eError = OMX_ErrorUndefined;

      eError = OMX_GetExtensionIndex (self->dec->handle,
         (OMX_STRING) NVX_INDEX_CONFIG_VIDEOPLANESINFO, &eIndex);

      if (eError == OMX_ErrorNone) {
        NVX_CONFIG_VIDEOPLANESINFO oInfo;

        GST_OMX_INIT_STRUCT (&oInfo);

        eError = OMX_GetConfig (self->dec->handle, eIndex, &oInfo);

        if (eError == OMX_ErrorNone) {
          offset[0] = 0;
          stride[0] = oInfo.nAlign[0][0];
          offset[1] = oInfo.nAlign[0][0] * oInfo.nAlign[0][1];
          stride[1] = oInfo.nAlign[1][0] << 1;
          offset[2] = offset[1] + stride[1] * oInfo.nAlign[1][1];
          stride[2] = oInfo.nAlign[2][0];
        }
      }

      if (eError != OMX_ErrorNone)
#endif
        switch (pool->video_info.finfo->format) {
          case GST_VIDEO_FORMAT_I420:
            offset[0] = 0;
            stride[0] = pool->port->port_def.format.video.nStride;
            offset[1] =
                stride[0] * pool->port->port_def.format.video.nSliceHeight;
            stride[1] = pool->port->port_def.format.video.nStride / 2;
            offset[2] =
                offset[1] +
                stride[1] * (pool->port->port_def.format.video.nSliceHeight /
                2);
            stride[2] = pool->port->port_def.format.video.nStride / 2;
            break;
          case GST_VIDEO_FORMAT_NV12:
            offset[0] = 0;
            stride[0] = pool->port->port_def.format.video.nStride;
            offset[1] =
                stride[0] * pool->port->port_def.format.video.nSliceHeight;
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
  }

  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (buf),
      gst_omx_buffer_data_quark, omx_buf, NULL);

  *buffer = buf;

  pool->current_buffer_index++;

  return GST_FLOW_OK;
}

static void
gst_omx_buffer_pool_free_buffer (GstBufferPool * bpool, GstBuffer * buffer)
{
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (bpool);

  /* If the buffers belong to another pool, restore them now */
  GST_OBJECT_LOCK (pool);
  if (pool->other_pool) {
    gst_object_replace ((GstObject **) & buffer->pool,
        (GstObject *) pool->other_pool);
  }
  GST_OBJECT_UNLOCK (pool);

  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (buffer),
      gst_omx_buffer_data_quark, NULL, NULL);

  GST_BUFFER_POOL_CLASS (gst_omx_buffer_pool_parent_class)->free_buffer (bpool,
      buffer);
}

static GstFlowReturn
gst_omx_buffer_pool_acquire_buffer (GstBufferPool * bpool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstFlowReturn ret;
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (bpool);

  if (pool->port->port_def.eDir == OMX_DirOutput) {
    GstBuffer *buf;

    g_return_val_if_fail (pool->current_buffer_index != -1, GST_FLOW_ERROR);

    buf = g_ptr_array_index (pool->buffers, pool->current_buffer_index);
    g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);
    *buffer = buf;
    ret = GST_FLOW_OK;

    /* If it's our own memory we have to set the sizes */
    if (!pool->other_pool) {
      GstMemory *mem = gst_buffer_peek_memory (*buffer, 0);

      g_assert (mem
          && g_strcmp0 (mem->allocator->mem_type, GST_OMX_MEMORY_TYPE) == 0);
      mem->size = ((GstOMXMemory *) mem)->buf->omx_buf->nFilledLen;
      mem->offset = ((GstOMXMemory *) mem)->buf->omx_buf->nOffset;
    } else {
#ifdef USE_OMX_TARGET_TEGRA
      GstMemory *mem = gst_buffer_peek_memory (buf, 0);
      if (mem
          && g_strcmp0 (mem->allocator->mem_type,
              GST_OMX_SINK_MEMORY_TYPE) == 0) {
        GstOMXBuffer *omxbuf =
            gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (buf),
            gst_omx_buffer_data_quark);
        mem->size = omxbuf->omx_buf->nFilledLen;
        mem->offset = omxbuf->omx_buf->nOffset;
        omxbuf =
            gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (buf),
            gst_omx_sink_data_quark);
        omxbuf->omx_buf->nFlags |= OMX_BUFFERFLAG_NV_BUFFER;
      }
#endif
    }
  } else {
    /* Acquire any buffer that is available to be filled by upstream */
    ret =
        GST_BUFFER_POOL_CLASS (gst_omx_buffer_pool_parent_class)->acquire_buffer
        (bpool, buffer, params);
  }

  return ret;
}

static void
gst_omx_buffer_pool_release_buffer (GstBufferPool * bpool, GstBuffer * buffer)
{
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (bpool);
  OMX_ERRORTYPE err;
  GstOMXBuffer *omx_buf;

  g_assert (pool->component && pool->port);

  if (!pool->allocating && !pool->deactivated) {
    omx_buf =
        gst_mini_object_get_qdata (GST_MINI_OBJECT_CAST (buffer),
        gst_omx_buffer_data_quark);
    if (pool->port->port_def.eDir == OMX_DirOutput && !omx_buf->used) {
      /* Release back to the port, can be filled again */
      err = gst_omx_port_release_buffer (pool->port, omx_buf);
      if (err != OMX_ErrorNone) {
        GST_ELEMENT_ERROR (pool->element, LIBRARY, SETTINGS, (NULL),
            ("Failed to relase output buffer to component: %s (0x%08x)",
                gst_omx_error_to_string (err), err));
      }
    } else if (!omx_buf->used) {
      /* TODO: Implement.
       *
       * If not used (i.e. was not passed to the component) this should do
       * the same as EmptyBufferDone.
       * If it is used (i.e. was passed to the component) this should do
       * nothing until EmptyBufferDone.
       *
       * EmptyBufferDone should release the buffer to the pool so it can
       * be allocated again
       *
       * Needs something to call back here in EmptyBufferDone, like keeping
       * a ref on the buffer in GstOMXBuffer until EmptyBufferDone... which
       * would ensure that the buffer is always unused when this is called.
       */
      g_assert_not_reached ();
      GST_BUFFER_POOL_CLASS (gst_omx_buffer_pool_parent_class)->release_buffer
          (bpool, buffer);
    }
  }
}

static void
gst_omx_buffer_pool_finalize (GObject * object)
{
  GstOMXBufferPool *pool = GST_OMX_BUFFER_POOL (object);

  if (pool->element)
    gst_object_unref (pool->element);
  pool->element = NULL;

  if (pool->buffers)
    g_ptr_array_unref (pool->buffers);
  pool->buffers = NULL;

  if (pool->other_pool)
    gst_object_unref (pool->other_pool);
  pool->other_pool = NULL;

  if (pool->allocator)
    gst_object_unref (pool->allocator);
  pool->allocator = NULL;

  if (pool->caps)
    gst_caps_unref (pool->caps);
  pool->caps = NULL;

  G_OBJECT_CLASS (gst_omx_buffer_pool_parent_class)->finalize (object);
}

static void
gst_omx_buffer_pool_class_init (GstOMXBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gst_omx_buffer_data_quark = g_quark_from_static_string ("GstOMXBufferData");

  gobject_class->finalize = gst_omx_buffer_pool_finalize;
  gstbufferpool_class->start = gst_omx_buffer_pool_start;
  gstbufferpool_class->stop = gst_omx_buffer_pool_stop;
  gstbufferpool_class->get_options = gst_omx_buffer_pool_get_options;
  gstbufferpool_class->set_config = gst_omx_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = gst_omx_buffer_pool_alloc_buffer;
  gstbufferpool_class->free_buffer = gst_omx_buffer_pool_free_buffer;
  gstbufferpool_class->acquire_buffer = gst_omx_buffer_pool_acquire_buffer;
  gstbufferpool_class->release_buffer = gst_omx_buffer_pool_release_buffer;
}

static void
gst_omx_buffer_pool_init (GstOMXBufferPool * pool)
{
  pool->buffers = g_ptr_array_new ();
  pool->allocator = g_object_new (gst_omx_memory_allocator_get_type (), NULL);
}

static GstBufferPool *
gst_omx_buffer_pool_new (GstElement * element, GstOMXComponent * component,
    GstOMXPort * port)
{
  GstOMXBufferPool *pool;

  pool = g_object_new (gst_omx_buffer_pool_get_type (), NULL);
  pool->element = gst_object_ref (element);
  pool->component = component;
  pool->port = port;

  return GST_BUFFER_POOL (pool);
}

typedef struct _BufferIdentification BufferIdentification;
struct _BufferIdentification
{
  guint64 timestamp;
};

static void
buffer_identification_free (BufferIdentification * id)
{
  g_slice_free (BufferIdentification, id);
}

/* prototypes */
static void gst_omx_video_dec_finalize (GObject * object);

static GstStateChangeReturn
gst_omx_video_dec_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_omx_video_dec_open (GstVideoDecoder * decoder);
static gboolean gst_omx_video_dec_close (GstVideoDecoder * decoder);
static gboolean gst_omx_video_dec_start (GstVideoDecoder * decoder);
static gboolean gst_omx_video_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_omx_video_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_omx_video_dec_reset (GstVideoDecoder * decoder,
    gboolean hard);
static GstFlowReturn gst_omx_video_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_omx_video_dec_finish (GstVideoDecoder * decoder);
static gboolean gst_omx_video_dec_decide_allocation (GstVideoDecoder * bdec,
    GstQuery * query);

static GstFlowReturn gst_omx_video_dec_drain (GstOMXVideoDec * self,
    gboolean is_eos);

static OMX_ERRORTYPE gst_omx_video_dec_allocate_output_buffers (GstOMXVideoDec *
    self);
static OMX_ERRORTYPE gst_omx_video_dec_deallocate_output_buffers (GstOMXVideoDec
    * self);

enum
{
  PROP_0,
#ifdef USE_OMX_TARGET_TEGRA
  PROP_USE_OMXDEC_RES,
  PROP_USE_FULL_FRAME,
  PROP_DISABLE_DPB,
  PROP_SKIP_FRAME
#endif
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_video_dec_debug_category, "omxvideodec", 0, \
      "debug category for gst-omx video decoder base class");


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstOMXVideoDec, gst_omx_video_dec,
    GST_TYPE_VIDEO_DECODER, DEBUG_INIT);

static void
gst_omx_video_dec_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOMXVideoDec *self = GST_OMX_VIDEO_DEC (object);

  switch (prop_id) {
#ifdef USE_OMX_TARGET_TEGRA
    case PROP_USE_OMXDEC_RES:
      self->use_omxdec_res = g_value_get_boolean (value);
      break;
    case PROP_USE_FULL_FRAME:
      self->full_frame_data = g_value_get_boolean (value);
      break;
    case PROP_DISABLE_DPB:
      self->disable_dpb = g_value_get_boolean (value);
      break;
    case PROP_SKIP_FRAME:
      self->skip_frames = g_value_get_enum (value);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_video_dec_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOMXVideoDec *self = GST_OMX_VIDEO_DEC (object);

  switch (prop_id) {
#ifdef USE_OMX_TARGET_TEGRA
    case PROP_USE_OMXDEC_RES:
      g_value_set_boolean (value, self->use_omxdec_res);
      break;
    case PROP_USE_FULL_FRAME:
      g_value_set_boolean (value, self->full_frame_data);
      break;
    case PROP_DISABLE_DPB:
      g_value_set_boolean (value, self->disable_dpb);
      break;
    case PROP_SKIP_FRAME:
      g_value_set_enum (value, self->skip_frames);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_video_dec_class_init (GstOMXVideoDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *video_decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  gobject_class->finalize = gst_omx_video_dec_finalize;

  gobject_class->set_property = gst_omx_video_dec_set_property;
  gobject_class->get_property = gst_omx_video_dec_get_property;

#ifdef USE_OMX_TARGET_TEGRA
  g_object_class_install_property (gobject_class, PROP_USE_OMXDEC_RES,
      g_param_spec_boolean ("use-omxdec-res",
          "Use resolution from omx decoder(for debugging purpose)",
          "Omx decoder resolution to be used(for debugging purpose)",
          DEFAULT_USE_OMXDEC_RES, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USE_FULL_FRAME,
      g_param_spec_boolean ("full-frame",
          "Full Frame data",
          "Whether or not the data is full framed",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DISABLE_DPB,
      g_param_spec_boolean ("disable-dpb",
          "Disable H.264 DPB buffer",
          "Set to disable H.264 DPB buffer for low latency",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SKIP_FRAME,
      g_param_spec_enum ("skip-frames",
          "Skip frames",
          "Which type of frames to skip during decoding",
          GST_TYPE_OMX_VID_DEC_SKIP_FRAMES,
          DEFAULT_SKIP_FRAME_TYPE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
#endif

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_video_dec_change_state);

  video_decoder_class->open = GST_DEBUG_FUNCPTR (gst_omx_video_dec_open);
  video_decoder_class->close = GST_DEBUG_FUNCPTR (gst_omx_video_dec_close);
  video_decoder_class->start = GST_DEBUG_FUNCPTR (gst_omx_video_dec_start);
  video_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_omx_video_dec_stop);
  video_decoder_class->reset = GST_DEBUG_FUNCPTR (gst_omx_video_dec_reset);
  video_decoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_omx_video_dec_set_format);
  video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_omx_video_dec_handle_frame);
  video_decoder_class->finish = GST_DEBUG_FUNCPTR (gst_omx_video_dec_finish);
  video_decoder_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_omx_video_dec_decide_allocation);

  klass->cdata.type = GST_OMX_COMPONENT_TYPE_FILTER;
  klass->cdata.default_src_template_caps =
#if (defined (USE_OMX_TARGET_RPI) || defined (USE_OMX_TARGET_TEGRA)) && defined (HAVE_GST_GL)
      GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
      "RGBA") ", texture-target=2D " "; "
#endif
      "video/x-raw, "
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", " "framerate = " GST_VIDEO_FPS_RANGE;
}

OMX_ERRORTYPE
gst_omx_set_full_frame_data_property (OMX_HANDLETYPE omx_handle)
{
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  OMX_CONFIG_BOOLEANTYPE full_frame_data;
  GST_OMX_INIT_STRUCT (&full_frame_data);
  eError =
      OMX_GetExtensionIndex (omx_handle,
      (OMX_STRING) "OMX.Nvidia.index.param.vdecfullframedata", &eIndex);
  if (eError == OMX_ErrorNone) {
    full_frame_data.bEnabled = OMX_TRUE;
    OMX_SetParameter (omx_handle, eIndex, &full_frame_data);
  }
  return eError;
}

static void
gst_omx_video_dec_init (GstOMXVideoDec * self)
{
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);

#ifdef USE_OMX_TARGET_TEGRA
  self->use_omxdec_res = DEFAULT_USE_OMXDEC_RES;
  self->full_frame_data = FALSE;
#endif

  g_mutex_init (&self->drain_lock);
  g_cond_init (&self->drain_cond);
}

static gboolean
gst_omx_video_dec_open (GstVideoDecoder * decoder)
{
  GstOMXVideoDec *self = GST_OMX_VIDEO_DEC (decoder);
  GstOMXVideoDecClass *klass = GST_OMX_VIDEO_DEC_GET_CLASS (self);
  gint in_port_index, out_port_index;

  GST_DEBUG_OBJECT (self, "Opening decoder");

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
        gst_omx_component_get_parameter (self->dec, OMX_IndexParamVideoInit,
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

#ifdef USE_OMX_TARGET_TEGRA
  {
    OMX_PARAM_PORTDEFINITIONTYPE port_def;

    gst_omx_port_get_port_definition (self->dec_in_port, &port_def);
    port_def.format.video.nFrameWidth = port_def.format.video.nFrameHeight = 0;
    gst_omx_port_update_port_definition (self->dec_in_port, &port_def);
  }
#endif

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  GST_DEBUG_OBJECT (self, "Opening EGL renderer");
  self->egl_render =
      gst_omx_component_new (GST_OBJECT_CAST (self), klass->cdata.core_name,
      "OMX.broadcom.egl_render", NULL, klass->cdata.hacks);

  if (!self->egl_render)
    return FALSE;

  if (gst_omx_component_get_state (self->egl_render,
          GST_CLOCK_TIME_NONE) != OMX_StateLoaded)
    return FALSE;

  {
    OMX_PORT_PARAM_TYPE param;
    OMX_ERRORTYPE err;

    GST_OMX_INIT_STRUCT (&param);

    err =
        gst_omx_component_get_parameter (self->egl_render,
        OMX_IndexParamVideoInit, &param);
    if (err != OMX_ErrorNone) {
      GST_WARNING_OBJECT (self, "Couldn't get port information: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      /* Fallback */
      in_port_index = 0;
      out_port_index = 1;
    } else {
      GST_DEBUG_OBJECT (self, "Detected %u ports, starting at %u", param.nPorts,
          param.nStartPortNumber);
      in_port_index = param.nStartPortNumber + 0;
      out_port_index = param.nStartPortNumber + 1;
    }
  }

  self->egl_in_port =
      gst_omx_component_add_port (self->egl_render, in_port_index);
  self->egl_out_port =
      gst_omx_component_add_port (self->egl_render, out_port_index);

  if (!self->egl_in_port || !self->egl_out_port)
    return FALSE;

  GST_DEBUG_OBJECT (self, "Opened EGL renderer");
#endif

  return TRUE;
}

static gboolean
gst_omx_video_dec_shutdown (GstOMXVideoDec * self)
{
  OMX_STATETYPE state;

  GST_DEBUG_OBJECT (self, "Shutting down decoder");

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  state = gst_omx_component_get_state (self->egl_render, 0);
  if (state > OMX_StateLoaded || state == OMX_StateInvalid) {
    if (state > OMX_StateIdle) {
      gst_omx_component_set_state (self->egl_render, OMX_StateIdle);
      gst_omx_component_set_state (self->dec, OMX_StateIdle);
      gst_omx_component_get_state (self->egl_render, 5 * GST_SECOND);
      gst_omx_component_get_state (self->dec, 1 * GST_SECOND);
    }
    gst_omx_component_set_state (self->egl_render, OMX_StateLoaded);
    gst_omx_component_set_state (self->dec, OMX_StateLoaded);

    gst_omx_port_deallocate_buffers (self->dec_in_port);
    gst_omx_video_dec_deallocate_output_buffers (self);
    gst_omx_component_close_tunnel (self->dec, self->dec_out_port,
        self->egl_render, self->egl_in_port);
    if (state > OMX_StateLoaded) {
      gst_omx_component_get_state (self->egl_render, 5 * GST_SECOND);
      gst_omx_component_get_state (self->dec, 1 * GST_SECOND);
    }
  }

  /* Otherwise we didn't use EGL and just fall back to
   * shutting down the decoder */
#endif

  state = gst_omx_component_get_state (self->dec, 0);
  if (state > OMX_StateLoaded || state == OMX_StateInvalid) {
    if (state > OMX_StateIdle) {
      gst_omx_component_set_state (self->dec, OMX_StateIdle);
      gst_omx_component_get_state (self->dec, 5 * GST_SECOND);
    }
    gst_omx_component_set_state (self->dec, OMX_StateLoaded);
    gst_omx_port_deallocate_buffers (self->dec_in_port);
    gst_omx_video_dec_deallocate_output_buffers (self);
    if (state > OMX_StateLoaded)
      gst_omx_component_get_state (self->dec, 5 * GST_SECOND);
  }

  return TRUE;
}

static gboolean
gst_omx_video_dec_close (GstVideoDecoder * decoder)
{
  GstOMXVideoDec *self = GST_OMX_VIDEO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Closing decoder");

  if (!gst_omx_video_dec_shutdown (self))
    return FALSE;

  self->dec_in_port = NULL;
  self->dec_out_port = NULL;
  if (self->dec)
    gst_omx_component_free (self->dec);
  self->dec = NULL;

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  self->egl_in_port = NULL;
  self->egl_out_port = NULL;
  if (self->egl_render)
    gst_omx_component_free (self->egl_render);
  self->egl_render = NULL;
#endif

  self->started = FALSE;

  GST_DEBUG_OBJECT (self, "Closed decoder");

  return TRUE;
}

static void
gst_omx_video_dec_finalize (GObject * object)
{
  GstOMXVideoDec *self = GST_OMX_VIDEO_DEC (object);

  g_mutex_clear (&self->drain_lock);
  g_cond_clear (&self->drain_cond);

  G_OBJECT_CLASS (gst_omx_video_dec_parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_omx_video_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstOMXVideoDec *self;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  g_return_val_if_fail (GST_IS_OMX_VIDEO_DEC (element),
      GST_STATE_CHANGE_FAILURE);
  self = GST_OMX_VIDEO_DEC (element);

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
#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
      if (self->egl_in_port)
        gst_omx_port_set_flushing (self->egl_in_port, 5 * GST_SECOND, TRUE);
      if (self->egl_out_port)
        gst_omx_port_set_flushing (self->egl_out_port, 5 * GST_SECOND, TRUE);
#endif

      g_mutex_lock (&self->drain_lock);
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
      g_mutex_unlock (&self->drain_lock);
      break;
    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  ret =
      GST_ELEMENT_CLASS (gst_omx_video_dec_parent_class)->change_state
      (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->downstream_flow_ret = GST_FLOW_FLUSHING;
      self->started = FALSE;

      if (!gst_omx_video_dec_shutdown (self))
        ret = GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

#define MAX_FRAME_DIST_TICKS  (5 * OMX_TICKS_PER_SECOND)
#define MAX_FRAME_DIST_FRAMES (100)

static GstVideoCodecFrame *
_find_nearest_frame (GstOMXVideoDec * self, GstOMXBuffer * buf)
{
  GList *l, *best_l = NULL;
  GList *finish_frames = NULL;
  GstVideoCodecFrame *best = NULL;
  guint64 best_timestamp = 0;
  guint64 best_diff = G_MAXUINT64;
  BufferIdentification *best_id = NULL;
  GList *frames;

  frames = gst_video_decoder_get_frames (GST_VIDEO_DECODER (self));

  for (l = frames; l; l = l->next) {
    GstVideoCodecFrame *tmp = l->data;
    BufferIdentification *id = gst_video_codec_frame_get_user_data (tmp);
    guint64 timestamp, diff;

    /* This happens for frames that were just added but
     * which were not passed to the component yet. Ignore
     * them here!
     */
    if (!id)
      continue;

    timestamp = id->timestamp;

    if (timestamp > buf->omx_buf->nTimeStamp)
      diff = timestamp - buf->omx_buf->nTimeStamp;
    else
      diff = buf->omx_buf->nTimeStamp - timestamp;

    if (best == NULL || diff < best_diff) {
      best = tmp;
      best_timestamp = timestamp;
      best_diff = diff;
      best_l = l;
      best_id = id;

      /* For frames without timestamp we simply take the first frame */
      if ((buf->omx_buf->nTimeStamp == 0 && timestamp == 0) || diff == 0)
        break;
    }
  }

  if (FALSE && best_id) {
    for (l = frames; l && l != best_l; l = l->next) {
      GstVideoCodecFrame *tmp = l->data;
      BufferIdentification *id = gst_video_codec_frame_get_user_data (tmp);
      guint64 diff_ticks, diff_frames;

      /* This happens for frames that were just added but
       * which were not passed to the component yet. Ignore
       * them here!
       */
      if (!id)
        continue;

      if (id->timestamp > best_timestamp)
        break;

      if (id->timestamp == 0 || best_timestamp == 0)
        diff_ticks = 0;
      else
        diff_ticks = best_timestamp - id->timestamp;
      diff_frames = best->system_frame_number - tmp->system_frame_number;

      if (diff_ticks > MAX_FRAME_DIST_TICKS
          || diff_frames > MAX_FRAME_DIST_FRAMES) {
        finish_frames =
            g_list_prepend (finish_frames, gst_video_codec_frame_ref (tmp));
      }
    }
  }

  if (FALSE && finish_frames) {
    g_warning ("Too old frames, bug in decoder -- please file a bug");
    for (l = finish_frames; l; l = l->next) {
      gst_video_decoder_drop_frame (GST_VIDEO_DECODER (self), l->data);
    }
  }

  if (best)
    gst_video_codec_frame_ref (best);

  g_list_foreach (frames, (GFunc) gst_video_codec_frame_unref, NULL);
  g_list_free (frames);

  return best;
}

static gboolean
gst_omx_video_dec_fill_buffer (GstOMXVideoDec * self,
    GstOMXBuffer * inbuf, GstBuffer * outbuf)
{
  GstVideoCodecState *state =
      gst_video_decoder_get_output_state (GST_VIDEO_DECODER (self));
  GstVideoInfo *vinfo = &state->info;
  OMX_PARAM_PORTDEFINITIONTYPE *port_def = &self->dec_out_port->port_def;
  gboolean ret = FALSE;
  GstVideoFrame frame;

  if (vinfo->width != port_def->format.video.nFrameWidth ||
      vinfo->height != port_def->format.video.nFrameHeight) {
    GST_ERROR_OBJECT (self, "Resolution do not match: port=%ux%u vinfo=%dx%d",
        (guint) port_def->format.video.nFrameWidth,
        (guint) port_def->format.video.nFrameHeight,
        vinfo->width, vinfo->height);
    goto done;
  }

  /* Same strides and everything */
  if (gst_buffer_get_size (outbuf) == inbuf->omx_buf->nFilledLen) {
    GstMapInfo map = GST_MAP_INFO_INIT;

    gst_buffer_map (outbuf, &map, GST_MAP_WRITE);
    memcpy (map.data,
        inbuf->omx_buf->pBuffer + inbuf->omx_buf->nOffset,
        inbuf->omx_buf->nFilledLen);
    gst_buffer_unmap (outbuf, &map);
    ret = TRUE;
    goto done;
  }

  /* Different strides */
#ifdef USE_OMX_TARGET_TEGRA
  /*TODO: get videoplanesinfo() for TEGRA, currently this path is not used */
  return FALSE;
#endif

  switch (vinfo->finfo->format) {
    case GST_VIDEO_FORMAT_I420:{
      gint i, j, height, width;
      guint8 *src, *dest;
      gint src_stride, dest_stride;

      gst_video_frame_map (&frame, vinfo, outbuf, GST_MAP_WRITE);
      for (i = 0; i < 3; i++) {
        if (i == 0) {
          src_stride = port_def->format.video.nStride;
          dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, i);

          /* XXX: Try this if no stride was set */
          if (src_stride == 0)
            src_stride = dest_stride;
        } else {
          src_stride = port_def->format.video.nStride / 2;
          dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, i);

          /* XXX: Try this if no stride was set */
          if (src_stride == 0)
            src_stride = dest_stride;
        }

        src = inbuf->omx_buf->pBuffer + inbuf->omx_buf->nOffset;
        if (i > 0)
          src +=
              port_def->format.video.nSliceHeight *
              port_def->format.video.nStride;
        if (i == 2)
          src +=
              (port_def->format.video.nSliceHeight / 2) *
              (port_def->format.video.nStride / 2);

        dest = GST_VIDEO_FRAME_COMP_DATA (&frame, i);
        height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, i);
        width = GST_VIDEO_FRAME_COMP_WIDTH (&frame, i);

        for (j = 0; j < height; j++) {
          memcpy (dest, src, width);
          src += src_stride;
          dest += dest_stride;
        }
      }
      gst_video_frame_unmap (&frame);
      ret = TRUE;
      break;
    }
    case GST_VIDEO_FORMAT_NV12:{
      gint i, j, height, width;
      guint8 *src, *dest;
      gint src_stride, dest_stride;

      gst_video_frame_map (&frame, vinfo, outbuf, GST_MAP_WRITE);
      for (i = 0; i < 2; i++) {
        if (i == 0) {
          src_stride = port_def->format.video.nStride;
          dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, i);

          /* XXX: Try this if no stride was set */
          if (src_stride == 0)
            src_stride = dest_stride;
        } else {
          src_stride = port_def->format.video.nStride;
          dest_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, i);

          /* XXX: Try this if no stride was set */
          if (src_stride == 0)
            src_stride = dest_stride;
        }

        src = inbuf->omx_buf->pBuffer + inbuf->omx_buf->nOffset;
        if (i == 1)
          src +=
              port_def->format.video.nSliceHeight *
              port_def->format.video.nStride;

        dest = GST_VIDEO_FRAME_COMP_DATA (&frame, i);
        height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, i);
        width = GST_VIDEO_FRAME_COMP_WIDTH (&frame, i) * (i == 0 ? 1 : 2);

        for (j = 0; j < height; j++) {
          memcpy (dest, src, width);
          src += src_stride;
          dest += dest_stride;
        }
      }
      gst_video_frame_unmap (&frame);
      ret = TRUE;
      break;
    }
    default:
      GST_ERROR_OBJECT (self, "Unsupported format");
      goto done;
      break;
  }


done:
  if (ret) {
    GST_BUFFER_PTS (outbuf) =
        gst_util_uint64_scale (inbuf->omx_buf->nTimeStamp, GST_SECOND,
        OMX_TICKS_PER_SECOND);
    if (inbuf->omx_buf->nTickCount != 0)
      GST_BUFFER_DURATION (outbuf) =
          gst_util_uint64_scale (inbuf->omx_buf->nTickCount, GST_SECOND,
          OMX_TICKS_PER_SECOND);
  }

  gst_video_codec_state_unref (state);

  return ret;
}

static gfloat y_invert_matrix[] = {
  1.0, 0.0, 0.0, 0.0,
  0.0, -1.0, 0.0, 1.0,
  0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 1.0,
};

static OMX_ERRORTYPE
gst_omx_video_dec_allocate_output_buffers (GstOMXVideoDec * self)
{
  OMX_ERRORTYPE err = OMX_ErrorNone;
  GstOMXPort *port;
  GstBufferPool *pool;
  GstStructure *config;
  gboolean eglimage = FALSE, add_videometa = FALSE;
  gboolean shared_buffer = FALSE;
  GstCaps *caps = NULL;
  guint min = 0, max = 0;
  GstVideoCodecState *state =
      gst_video_decoder_get_output_state (GST_VIDEO_DECODER (self));

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  port = self->eglimage ? self->egl_out_port : self->dec_out_port;
#else
  port = self->dec_out_port;
#endif

  pool = gst_video_decoder_get_buffer_pool (GST_VIDEO_DECODER (self));
  if (pool) {
    GstAllocator *allocator;

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &caps, NULL, &min, &max);
    gst_buffer_pool_config_get_allocator (config, &allocator, NULL);

    /* Need at least 2 buffers for anything meaningful */
    min = MAX (MAX (min, port->port_def.nBufferCountMin), 4);
    if (max == 0) {
      max = min;
    } else if (max < port->port_def.nBufferCountMin || max < 2) {
      /* Can't use pool because can't have enough buffers */
      gst_caps_replace (&caps, NULL);
    } else {
      min = max;
    }

    add_videometa = gst_buffer_pool_config_has_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

#if (defined (USE_OMX_TARGET_RPI) || defined (USE_OMX_TARGET_TEGRA)) && defined (HAVE_GST_GL)
    eglimage = self->eglimage
        && ((allocator && GST_IS_GL_MEMORY_EGL_ALLOCATOR (allocator))
            || (pool && GST_IS_GL_BUFFER_POOL (pool)));
#else
    /* TODO: Implement something that works for other targets too */
    eglimage = FALSE;
#endif

#if defined (USE_OMX_TARGET_TEGRA)
    if (!eglimage) {
      shared_buffer = (allocator
          && g_strcmp0 (allocator->mem_type, GST_OMX_SINK_MEMORY_TYPE) == 0);
    }
#endif

    caps = caps ? gst_caps_ref (caps) : NULL;

    GST_DEBUG_OBJECT (self, "Trying to use pool %p with caps %" GST_PTR_FORMAT
        " and memory type %s", pool, caps,
        (allocator ? allocator->mem_type : "(null)"));
  } else {
    gst_caps_replace (&caps, NULL);
    min = max = port->port_def.nBufferCountMin;
    GST_DEBUG_OBJECT (self, "No pool available, not negotiated yet");
  }

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  /* Will retry without EGLImage */
  if (self->eglimage && !eglimage) {
    GST_DEBUG_OBJECT (self,
        "Wanted to use EGLImage but downstream doesn't support it");
    err = OMX_ErrorUndefined;
    goto done;
  }
#endif

  if (caps)
    self->out_port_pool =
        gst_omx_buffer_pool_new (GST_ELEMENT_CAST (self), self->dec, port);

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  if (eglimage) {
    GList *buffers = NULL;
    GList *images = NULL;
    gint i;
    GstBufferPoolAcquireParams params = { 0, };
    EGLDisplay egl_display = EGL_NO_DISPLAY;

    GST_DEBUG_OBJECT (self, "Trying to allocate %d EGLImages", min);

    for (i = 0; i < min; i++) {
      GstBuffer *buffer;
      GstMemory *mem;
      GstGLMemoryEGL *gl_mem;

      if (gst_buffer_pool_acquire_buffer (pool, &buffer, &params) != GST_FLOW_OK
          || gst_buffer_n_memory (buffer) != 1
          || !(mem = gst_buffer_peek_memory (buffer, 0))
          || !GST_IS_GL_MEMORY_EGL_ALLOCATOR (mem->allocator)) {
        GST_INFO_OBJECT (self, "Failed to allocated %d-th EGLImage", i);
        g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
        g_list_free (images);
        buffers = NULL;
        images = NULL;
        /* TODO: For non-RPi targets we want to use the normal memory code below */
        /* Retry without EGLImage */
        err = OMX_ErrorUndefined;
        goto done;
      }
      gl_mem = (GstGLMemoryEGL *) mem;
      buffers = g_list_append (buffers, buffer);
      /* FIXME: replace with the affine transformation meta */
/*      gl_mem->image->orientation =
          GST_VIDEO_GL_TEXTURE_ORIENTATION_X_NORMAL_Y_FLIP;*/
      images = g_list_append (images, gst_gl_memory_egl_get_image (gl_mem));
      if (egl_display == EGL_NO_DISPLAY)
        egl_display = gst_gl_memory_egl_get_display (gl_mem);
    }

    GST_DEBUG_OBJECT (self, "Allocated %d EGLImages successfully", min);

    /* Everything went fine? */
    if (eglimage) {
      GST_DEBUG_OBJECT (self, "Setting EGLDisplay");
      self->egl_out_port->port_def.format.video.pNativeWindow = egl_display;
      err =
          gst_omx_port_update_port_definition (self->egl_out_port,
          &self->egl_out_port->port_def);
      if (err != OMX_ErrorNone) {
        GST_INFO_OBJECT (self,
            "Failed to set EGLDisplay on port: %s (0x%08x)",
            gst_omx_error_to_string (err), err);
        g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
        g_list_free (images);
        /* TODO: For non-RPi targets we want to use the normal memory code below */
        /* Retry without EGLImage */
        goto done;
      } else {
        GList *l;

        if (min != port->port_def.nBufferCountActual) {
          err = gst_omx_port_update_port_definition (port, NULL);
          if (err == OMX_ErrorNone) {
            port->port_def.nBufferCountActual = min;
            err = gst_omx_port_update_port_definition (port, &port->port_def);
          }

          if (err != OMX_ErrorNone) {
            GST_INFO_OBJECT (self,
                "Failed to configure %u output buffers: %s (0x%08x)", min,
                gst_omx_error_to_string (err), err);
            g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
            g_list_free (images);
            /* TODO: For non-RPi targets we want to use the normal memory code below */
            /* Retry without EGLImage */

            goto done;
          }
        }

        if (!gst_omx_port_is_enabled (port)) {
          err = gst_omx_port_set_enabled (port, TRUE);
          if (err != OMX_ErrorNone) {
            GST_INFO_OBJECT (self,
                "Failed to enable port: %s (0x%08x)",
                gst_omx_error_to_string (err), err);
            g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
            g_list_free (images);
            /* TODO: For non-RPi targets we want to use the normal memory code below */
            /* Retry without EGLImage */
            goto done;
          }
        }

        err = gst_omx_port_use_eglimages (port, images);
        g_list_free (images);

        if (err != OMX_ErrorNone) {
          GST_INFO_OBJECT (self,
              "Failed to pass EGLImages to port: %s (0x%08x)",
              gst_omx_error_to_string (err), err);
          g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
          /* TODO: For non-RPi targets we want to use the normal memory code below */
          /* Retry without EGLImage */
          goto done;
        }

        err = gst_omx_port_wait_enabled (port, 2 * GST_SECOND);
        if (err != OMX_ErrorNone) {
          GST_INFO_OBJECT (self,
              "Failed to wait until port is enabled: %s (0x%08x)",
              gst_omx_error_to_string (err), err);
          g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
          /* TODO: For non-RPi targets we want to use the normal memory code below */
          /* Retry without EGLImage */
          goto done;
        }

        GST_DEBUG_OBJECT (self, "Populating internal buffer pool");
        GST_OMX_BUFFER_POOL (self->out_port_pool)->other_pool =
            GST_BUFFER_POOL (gst_object_ref (pool));
        for (l = buffers; l; l = l->next) {
          g_ptr_array_add (GST_OMX_BUFFER_POOL (self->out_port_pool)->buffers,
              l->data);
        }
        g_list_free (buffers);
        /* All good and done, set caps below */
      }
    }
  }
#elif defined (USE_OMX_TARGET_TEGRA)
  if (eglimage || shared_buffer) {
    GList *buffers = NULL;
    GList *pBuffers = NULL;
    GList *l;
    gint i;
    GstBufferPoolAcquireParams params = { 0, };
    GstMapInfo map = GST_MAP_INFO_INIT;
    GList *images = NULL;

    if (eglimage) {
      GST_DEBUG_OBJECT (self, "Trying to allocate %d EGLImages", min);

      for (i = 0; i < min; i++) {
        GstBuffer *buffer;
        GstMemory *mem;

        if (gst_buffer_pool_acquire_buffer (pool, &buffer,
                &params) != GST_FLOW_OK || gst_buffer_n_memory (buffer) != 1
            || !(mem = gst_buffer_peek_memory (buffer, 0))
            || !GST_IS_GL_MEMORY_EGL_ALLOCATOR (mem->allocator)) {
          GST_INFO_OBJECT (self, "Failed to allocated %d-th EGLImage", i);
          g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
          g_list_free (images);
          buffers = NULL;
          images = NULL;
          err = OMX_ErrorUndefined;
          goto done;
        }

        if (self->have_affine_transformation_meta) {
          GstVideoAffineTransformationMeta *af_meta;

          af_meta = gst_buffer_add_video_affine_transformation_meta (buffer);
          gst_video_affine_transformation_meta_apply_matrix (af_meta, y_invert_matrix);
        } else {
          GstTagList * tl;

          tl = gst_tag_list_new ("image-orientation", "flip-rotate-180", NULL);

          gst_video_decoder_merge_tags (GST_VIDEO_DECODER (self), tl,
              GST_TAG_MERGE_REPLACE);
        }

        buffers = g_list_append (buffers, buffer);
        images = g_list_append (images, gst_gl_memory_egl_get_image ((GstGLMemoryEGL *) mem));
      }

      GST_DEBUG_OBJECT (self, "Allocated %d EGLImages successfully", min);
    } else {
      GST_DEBUG_OBJECT (self, "Trying to Use %d OMX Buffers", min);

      for (i = 0; i < min; i++) {
        GstBuffer *buffer;
        GstMemory *mem = NULL;

        if (gst_buffer_pool_acquire_buffer (pool, &buffer,
                &params) != GST_FLOW_OK || gst_buffer_n_memory (buffer) != 1
            || !(mem = gst_buffer_peek_memory (buffer, 0))
            || !(gst_memory_map (mem, &map, GST_MAP_WRITE))
            || g_strcmp0 (mem->allocator->mem_type,
                GST_OMX_SINK_MEMORY_TYPE) != 0) {
          GST_INFO_OBJECT (self, "Failed to use %d-th OMX Buffer", i);
          g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
          g_list_free (pBuffers);
          buffers = NULL;
          pBuffers = NULL;
          if (map.memory == mem)
            gst_memory_unmap (mem, &map);
          err = OMX_ErrorUndefined;
          goto done;
        }

        buffers = g_list_append (buffers, buffer);
        pBuffers = g_list_append (pBuffers, map.data);
        gst_memory_unmap (mem, &map);
      }

      GST_DEBUG_OBJECT (self, "Allocated %d OMX Buffers", min);
    }

    if (min != port->port_def.nBufferCountActual) {
      err = gst_omx_port_update_port_definition (port, NULL);
      if (err == OMX_ErrorNone) {
        port->port_def.nBufferCountActual = min;
        err = gst_omx_port_update_port_definition (port, &port->port_def);
      }

      if (err != OMX_ErrorNone) {
        GST_INFO_OBJECT (self,
            "Failed to configure %u output buffers: %s (0x%08x)", min,
            gst_omx_error_to_string (err), err);
        g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
        goto done;
      }
    }

    if (!gst_omx_port_is_enabled (port)) {

      NVX_PARAM_USENVBUFFER param;
      OMX_INDEXTYPE eIndex;

      GST_DEBUG_OBJECT (self, "Setting decoder to use Nvmm buffer");

      err = gst_omx_component_get_index (self->dec,
          (char *) NVX_INDEX_CONFIG_USENVBUFFER, &eIndex);

      if (!eglimage) {
        if (err == OMX_ErrorNone) {
          GST_OMX_INIT_STRUCT (&param);
          param.nPortIndex = self->dec_out_port->index;
          param.bUseNvBuffer = OMX_TRUE;
          err = gst_omx_component_set_parameter (self->dec, eIndex, &param);
        } else {
          GST_WARNING_OBJECT (self, "Coudn't get extension index for %s",
              (char *) NVX_INDEX_CONFIG_USENVBUFFER);
          goto done;
        }
      }

      if (err != OMX_ErrorNone) {
        GST_WARNING_OBJECT (self, "Couldn't use HW Accelerated path");
        goto done;
      }

      err = gst_omx_port_set_enabled (port, TRUE);
      if (err != OMX_ErrorNone) {
        GST_INFO_OBJECT (self,
            "Failed to enable port: %s (0x%08x)",
            gst_omx_error_to_string (err), err);
        g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
        goto done;
      }
    }

    if (eglimage)
      err = gst_omx_port_use_eglimages (port, images);
    else
      err = gst_omx_port_use_buffers (port, pBuffers);

    if (err != OMX_ErrorNone) {
      GST_INFO_OBJECT (self,
          "Failed to pass OMX Buffers to port: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
      goto done;
    }

    err = gst_omx_port_wait_enabled (port, 2 * GST_SECOND);
    if (err != OMX_ErrorNone) {
      GST_INFO_OBJECT (self,
          "Failed to wait until port is enabled: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      g_list_free_full (buffers, (GDestroyNotify) gst_buffer_unref);
      goto done;
    }

    GST_DEBUG_OBJECT (self, "Populating internal buffer pool");
    GST_OMX_BUFFER_POOL (self->out_port_pool)->other_pool =
        GST_BUFFER_POOL (gst_object_ref (pool));
    for (l = buffers; l; l = l->next) {
      g_ptr_array_add (GST_OMX_BUFFER_POOL (self->out_port_pool)->buffers,
          l->data);
    }
    g_list_free (buffers);
  }
#endif

  /* If not using EGLImage or trying to use EGLImage failed */
  if (!eglimage && !shared_buffer) {
    gboolean was_enabled = TRUE;

    if (min != port->port_def.nBufferCountActual) {
      err = gst_omx_port_update_port_definition (port, NULL);
      if (err == OMX_ErrorNone) {
        port->port_def.nBufferCountActual = min;
        err = gst_omx_port_update_port_definition (port, &port->port_def);
      }

      if (err != OMX_ErrorNone) {
        GST_ERROR_OBJECT (self,
            "Failed to configure %u output buffers: %s (0x%08x)", min,
            gst_omx_error_to_string (err), err);
        goto done;
      }
    }

    if (!gst_omx_port_is_enabled (port)) {
      err = gst_omx_port_set_enabled (port, TRUE);
      if (err != OMX_ErrorNone) {
        GST_INFO_OBJECT (self,
            "Failed to enable port: %s (0x%08x)",
            gst_omx_error_to_string (err), err);
        goto done;
      }
      was_enabled = FALSE;
    }

    err = gst_omx_port_allocate_buffers (port);
    if (err != OMX_ErrorNone && min > port->port_def.nBufferCountMin) {
      GST_ERROR_OBJECT (self,
          "Failed to allocate required number of buffers %d, trying less and copying",
          min);
      min = port->port_def.nBufferCountMin;

      if (!was_enabled)
        gst_omx_port_set_enabled (port, FALSE);

      if (min != port->port_def.nBufferCountActual) {
        err = gst_omx_port_update_port_definition (port, NULL);
        if (err == OMX_ErrorNone) {
          port->port_def.nBufferCountActual = min;
          err = gst_omx_port_update_port_definition (port, &port->port_def);
        }

        if (err != OMX_ErrorNone) {
          GST_ERROR_OBJECT (self,
              "Failed to configure %u output buffers: %s (0x%08x)", min,
              gst_omx_error_to_string (err), err);
          goto done;
        }
      }

      err = gst_omx_port_allocate_buffers (port);

      /* Can't provide buffers downstream in this case */
      gst_caps_replace (&caps, NULL);
    }

    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self, "Failed to allocate %d buffers: %s (0x%08x)", min,
          gst_omx_error_to_string (err), err);
      goto done;
    }

    if (!was_enabled) {
      err = gst_omx_port_wait_enabled (port, 2 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_ERROR_OBJECT (self,
            "Failed to wait until port is enabled: %s (0x%08x)",
            gst_omx_error_to_string (err), err);
        goto done;
      }
    }


  }

  err = OMX_ErrorNone;

  if (caps) {
    config = gst_buffer_pool_get_config (self->out_port_pool);

    if (add_videometa)
      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_META);

    gst_buffer_pool_config_set_params (config, caps,
        self->dec_out_port->port_def.nBufferSize, min, max);

    if (!gst_buffer_pool_set_config (self->out_port_pool, config)) {
      GST_INFO_OBJECT (self, "Failed to set config on internal pool");
      gst_object_unref (self->out_port_pool);
      self->out_port_pool = NULL;
      goto done;
    }

    GST_OMX_BUFFER_POOL (self->out_port_pool)->allocating = TRUE;
    /* This now allocates all the buffers */
    if (!gst_buffer_pool_set_active (self->out_port_pool, TRUE)) {
      GST_INFO_OBJECT (self, "Failed to activate internal pool");
      gst_object_unref (self->out_port_pool);
      self->out_port_pool = NULL;
    } else {
      GST_OMX_BUFFER_POOL (self->out_port_pool)->allocating = FALSE;
    }
  } else if (self->out_port_pool) {
    gst_object_unref (self->out_port_pool);
    self->out_port_pool = NULL;
  }

done:
  if (!self->out_port_pool && err == OMX_ErrorNone)
    GST_DEBUG_OBJECT (self,
        "Not using our internal pool and copying buffers for downstream");

  if (caps)
    gst_caps_unref (caps);
  if (pool)
    gst_object_unref (pool);
  if (state)
    gst_video_codec_state_unref (state);

  return err;
}

static OMX_ERRORTYPE
gst_omx_video_dec_deallocate_output_buffers (GstOMXVideoDec * self)
{
  OMX_ERRORTYPE err;

  if (self->out_port_pool) {
    gst_buffer_pool_set_active (self->out_port_pool, FALSE);
#if 0
    gst_buffer_pool_wait_released (self->out_port_pool);
#endif
    GST_OMX_BUFFER_POOL (self->out_port_pool)->deactivated = TRUE;
    gst_object_unref (self->out_port_pool);
    self->out_port_pool = NULL;
  }
#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  err =
      gst_omx_port_deallocate_buffers (self->eglimage ? self->
      egl_out_port : self->dec_out_port);
#else
  err = gst_omx_port_deallocate_buffers (self->dec_out_port);
#endif

  return err;
}

#if defined (USE_OMX_TARGET_TEGRA) && defined (HAVE_GST_GL)
static NvBufType
gst_omx_video_dec_negotiate_nv_caps (GstOMXVideoDec * self,
    GstVideoCodecState * state)
{
  GstPad *peer = NULL;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  peer = gst_pad_get_peer (GST_VIDEO_DECODER_SRC_PAD (self));

  if (peer) {
    GstCaps *icaps;
    GstCapsFeatures *ift;

    icaps = gst_video_info_to_caps (&state->info);
    icaps = gst_caps_make_writable (icaps);

    ift = gst_caps_features_from_string ("memory:EGLImage");
    gst_caps_set_features (icaps, 0, ift);

    if (gst_pad_query_accept_caps (peer, icaps)) {
      gst_caps_unref (icaps);
      gst_object_unref (peer);
      return BUF_EGL;
    } else {
      gst_caps_features_add (ift, "memory:NVMM");
      gst_caps_features_remove (ift, "memory:EGLImage");

      if (gst_pad_query_accept_caps (peer, icaps)) {
        gst_caps_unref (icaps);
        gst_object_unref (peer);
        if (!gst_omx_port_is_enabled (self->dec_out_port)) {
          NVX_PARAM_USENVBUFFER param;
          OMX_INDEXTYPE eIndex;

          GST_DEBUG_OBJECT (self, "Setting decoder to use Nvmm buffer");

          err = gst_omx_component_get_index (self->dec,
              (char *) NVX_INDEX_CONFIG_USENVBUFFER, &eIndex);

          if (err == OMX_ErrorNone) {
            GST_OMX_INIT_STRUCT (&param);
            param.nPortIndex = self->dec_out_port->index;
            param.bUseNvBuffer = OMX_TRUE;
            err = gst_omx_component_set_parameter (self->dec, eIndex, &param);
          } else {
            GST_WARNING_OBJECT (self, "Coudn't get extension index for %s",
                (char *) NVX_INDEX_CONFIG_USENVBUFFER);
            goto done;
          }

          if (err != OMX_ErrorNone) {
            GST_WARNING_OBJECT (self, "Couldn't use HW Accelerated path");
            goto done;
          }
        }
        return BUF_NVMM;
      }
    }

    gst_caps_unref (icaps);
    gst_object_unref (peer);
  }
done:
  return BUF_NB;
}
#endif

static OMX_ERRORTYPE
gst_omx_video_dec_reconfigure_output_port (GstOMXVideoDec * self)
{
  GstOMXPort *port;
  OMX_ERRORTYPE err;
  GstVideoCodecState *state;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  GstVideoFormat format;

#if defined (USE_OMX_TARGET_TEGRA) && defined (HAVE_GST_GL)
  NvBufType nv_buf = BUF_NB;
#endif

  /* At this point the decoder output port is disabled */

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  {
    OMX_STATETYPE egl_state;

    if (self->eglimage) {
      /* Nothing to do here, we could however fall back to non-EGLImage in theory */
      port = self->egl_out_port;
      err = OMX_ErrorNone;
      goto enable_port;
    } else {
      /* Set up egl_render */

      self->eglimage = TRUE;

      gst_omx_port_get_port_definition (self->dec_out_port, &port_def);
      GST_VIDEO_DECODER_STREAM_LOCK (self);
      state = gst_video_decoder_set_output_state (GST_VIDEO_DECODER (self),
          GST_VIDEO_FORMAT_RGBA, port_def.format.video.nFrameWidth,
          port_def.format.video.nFrameHeight, self->input_state);

      /* at this point state->caps is NULL */
      if (state->caps)
        gst_caps_unref (state->caps);
      state->caps = gst_video_info_to_caps (&state->info);
      gst_caps_set_features (state->caps, 0,
          gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, NULL));

      /* try to negotiate with caps feature */
      if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {

        GST_DEBUG_OBJECT (self,
            "Failed to negotiate with feature %s",
            GST_CAPS_FEATURE_MEMORY_GL_MEMORY);

        if (state->caps)
          gst_caps_replace (&state->caps, NULL);

        /* fallback: try to use EGLImage even if it is not in the caps feature */
        if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
          gst_video_codec_state_unref (state);
          GST_ERROR_OBJECT (self, "Failed to negotiate RGBA for EGLImage");
          GST_VIDEO_DECODER_STREAM_UNLOCK (self);
          goto no_egl;
        }
      }

      gst_video_codec_state_unref (state);
      GST_VIDEO_DECODER_STREAM_UNLOCK (self);

      /* Now link it all together */

      err = gst_omx_port_set_enabled (self->egl_in_port, FALSE);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_wait_enabled (self->egl_in_port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_set_enabled (self->egl_out_port, FALSE);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_wait_enabled (self->egl_out_port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto no_egl;

      {
#define OMX_IndexParamBrcmVideoEGLRenderDiscardMode 0x7f0000db
        OMX_CONFIG_PORTBOOLEANTYPE discardMode;
        memset (&discardMode, 0, sizeof (discardMode));
        discardMode.nSize = sizeof (discardMode);
        discardMode.nPortIndex = 220;
        discardMode.nVersion.nVersion = OMX_VERSION;
        discardMode.bEnabled = OMX_FALSE;
        if (gst_omx_component_set_parameter (self->egl_render,
                OMX_IndexParamBrcmVideoEGLRenderDiscardMode,
                &discardMode) != OMX_ErrorNone)
          goto no_egl;
#undef OMX_IndexParamBrcmVideoEGLRenderDiscardMode
      }

      err =
          gst_omx_component_setup_tunnel (self->dec, self->dec_out_port,
          self->egl_render, self->egl_in_port);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_set_enabled (self->egl_in_port, TRUE);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_component_set_state (self->egl_render, OMX_StateIdle);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_wait_enabled (self->egl_in_port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto no_egl;

      if (gst_omx_component_get_state (self->egl_render,
              GST_CLOCK_TIME_NONE) != OMX_StateIdle)
        goto no_egl;

      err = gst_omx_video_dec_allocate_output_buffers (self);
      if (err != OMX_ErrorNone)
        goto no_egl;

      if (gst_omx_component_set_state (self->egl_render,
              OMX_StateExecuting) != OMX_ErrorNone)
        goto no_egl;

      if (gst_omx_component_get_state (self->egl_render,
              GST_CLOCK_TIME_NONE) != OMX_StateExecuting)
        goto no_egl;

      err =
          gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, FALSE);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err =
          gst_omx_port_set_flushing (self->egl_in_port, 5 * GST_SECOND, FALSE);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err =
          gst_omx_port_set_flushing (self->egl_out_port, 5 * GST_SECOND, FALSE);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_populate (self->egl_out_port);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_set_enabled (self->dec_out_port, TRUE);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_wait_enabled (self->dec_out_port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_mark_reconfigured (self->dec_out_port);
      if (err != OMX_ErrorNone)
        goto no_egl;

      err = gst_omx_port_mark_reconfigured (self->egl_out_port);
      if (err != OMX_ErrorNone)
        goto no_egl;

      goto done;
    }

  no_egl:

    gst_omx_port_set_enabled (self->dec_out_port, FALSE);
    gst_omx_port_wait_enabled (self->dec_out_port, 1 * GST_SECOND);
    egl_state = gst_omx_component_get_state (self->egl_render, 0);
    if (egl_state > OMX_StateLoaded || egl_state == OMX_StateInvalid) {
      if (egl_state > OMX_StateIdle) {
        gst_omx_component_set_state (self->egl_render, OMX_StateIdle);
        gst_omx_component_get_state (self->egl_render, 5 * GST_SECOND);
      }
      gst_omx_component_set_state (self->egl_render, OMX_StateLoaded);

      gst_omx_video_dec_deallocate_output_buffers (self);
      gst_omx_component_close_tunnel (self->dec, self->dec_out_port,
          self->egl_render, self->egl_in_port);

      if (egl_state > OMX_StateLoaded) {
        gst_omx_component_get_state (self->egl_render, 5 * GST_SECOND);
      }
    }

    /* After this egl_render should be deactivated
     * and the decoder's output port disabled */
    self->eglimage = FALSE;
  }
#endif
  port = self->dec_out_port;

  /* Update caps */
  GST_VIDEO_DECODER_STREAM_LOCK (self);

  gst_omx_port_get_port_definition (port, &port_def);
  g_assert (port_def.format.video.eCompressionFormat == OMX_VIDEO_CodingUnused);

  switch (port_def.format.video.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatYUV420PackedPlanar:
      GST_DEBUG_OBJECT (self, "Output is I420 (%d)",
          port_def.format.video.eColorFormat);
      format = GST_VIDEO_FORMAT_I420;
      break;
    case OMX_COLOR_FormatYUV420SemiPlanar:
      GST_DEBUG_OBJECT (self, "Output is NV12 (%d)",
          port_def.format.video.eColorFormat);
      format = GST_VIDEO_FORMAT_NV12;
      break;
    default:
      GST_ERROR_OBJECT (self, "Unsupported color format: %d",
          port_def.format.video.eColorFormat);
      GST_VIDEO_DECODER_STREAM_UNLOCK (self);
      err = OMX_ErrorUndefined;
      goto done;
      break;
  }

#ifdef USE_OMX_TARGET_TEGRA
  if (!self->use_omxdec_res) {
    OMX_INDEXTYPE eIndex;
    OMX_ERRORTYPE eError;
    OMX_U32 frame_size;

    eError = OMX_GetExtensionIndex (self->dec->handle,
       (OMX_STRING) NVX_INDEX_CONFIG_VIDEOPLANESINFO, &eIndex);

    if (eError == OMX_ErrorNone) {
      NVX_CONFIG_VIDEOPLANESINFO oInfo;

      GST_OMX_INIT_STRUCT (&oInfo);

      eError = OMX_GetConfig (self->dec->handle, eIndex, &oInfo);
      if (eError == OMX_ErrorNone) {
        GstOMXPort *iport = self->dec_in_port;
        OMX_PARAM_PORTDEFINITIONTYPE iport_def;

        GST_OMX_INIT_STRUCT (&iport_def);
        gst_omx_port_get_port_definition (iport, &iport_def);

        if (iport_def.format.video.nFrameWidth &&
            iport_def.format.video.nFrameHeight) {
          switch (format) {
            case GST_VIDEO_FORMAT_NV12:
              oInfo.nAlign[0][0] =
                  GST_ROUND_UP_4 (iport_def.format.video.nFrameWidth);
              oInfo.nAlign[0][1] =
                  GST_ROUND_UP_2 (iport_def.format.video.nFrameHeight);
              oInfo.nAlign[1][0] =
                  (GST_ROUND_UP_4 (iport_def.format.video.nFrameWidth)) >> 1;
              oInfo.nAlign[1][1] =
                  (GST_ROUND_UP_2 (iport_def.format.video.nFrameHeight)) >> 1;
              frame_size =
                  oInfo.nAlign[0][0] * oInfo.nAlign[0][1] +
                  oInfo.nAlign[1][0] * oInfo.nAlign[1][1];
              break;
            default:
              eError = OMX_ErrorUnsupportedSetting;
              break;
          }

          if (eError == OMX_ErrorNone)
            eError = OMX_SetConfig (self->dec->handle, eIndex, &oInfo);
        } else
          eError = OMX_ErrorUnsupportedSetting;
      }
    }

    if (eError == OMX_ErrorNone) {
      port_def.nBufferSize = frame_size;
      gst_omx_port_update_port_definition (port, &port_def);
      gst_omx_port_get_port_definition (port, &port_def);

    } else
      g_warning ("omxvideodec: failed to set output video alignment %x",
          eError);

  }
#endif

  GST_DEBUG_OBJECT (self,
      "Setting output state: format %s, width %u, height %u",
      gst_video_format_to_string (format),
      (guint) port_def.format.video.nFrameWidth,
      (guint) port_def.format.video.nFrameHeight);

  state = gst_video_decoder_set_output_state (GST_VIDEO_DECODER (self),
      format, port_def.format.video.nFrameWidth,
      port_def.format.video.nFrameHeight, self->input_state);
#if defined (USE_OMX_TARGET_TEGRA) && defined (HAVE_GST_GL)
  {
    nv_buf = gst_omx_video_dec_negotiate_nv_caps (self, state);
    if (nv_buf == BUF_EGL) {
      state = gst_video_decoder_set_output_state (GST_VIDEO_DECODER (self),
          GST_VIDEO_FORMAT_RGBA, port_def.format.video.nFrameWidth,
          port_def.format.video.nFrameHeight, self->input_state);

      /* at this point state->caps is NULL */
      if (state->caps)
        gst_caps_unref (state->caps);
      state->caps = gst_video_info_to_caps (&state->info);
      gst_caps_set_features (state->caps, 0,
          gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, NULL));

      self->eglimage = FALSE;
      if (gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
        self->eglimage = TRUE;
      } else {
        GST_DEBUG_OBJECT (self,
            "Failed to negotiate with feature %s",
            GST_CAPS_FEATURE_MEMORY_GL_MEMORY);

        if (state->caps)
          gst_caps_replace (&state->caps, NULL);

        /* fallback: try to use EGLImage even if it is not in the caps feature */
        if (gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
          self->eglimage = TRUE;
        }
      }

      if (!self->eglimage)
        GST_ERROR_OBJECT (self, "Failed to negotiate RGBA for EGLImage");
    }
  }
#endif

  gst_video_codec_state_unref (state);

  GST_VIDEO_DECODER_STREAM_UNLOCK (self);

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
enable_port:
#endif
  err = gst_omx_video_dec_allocate_output_buffers (self);
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
gst_omx_video_dec_loop (GstOMXVideoDec * self)
{
  GstOMXPort *port;
  GstOMXBuffer *buf = NULL;
  GstVideoCodecFrame *frame;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstOMXAcquireBufferReturn acq_return;
  GstClockTimeDiff deadline;
  OMX_ERRORTYPE err;

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  port = self->eglimage ? self->egl_out_port : self->dec_out_port;
#else
  port = self->dec_out_port;
#endif

  acq_return = gst_omx_port_acquire_buffer (port, &buf);
  if (acq_return == GST_OMX_ACQUIRE_BUFFER_ERROR) {
    goto component_error;
  } else if (acq_return == GST_OMX_ACQUIRE_BUFFER_FLUSHING) {
    goto flushing;
  } else if (acq_return == GST_OMX_ACQUIRE_BUFFER_EOS) {
    goto eos;
  }

  if (!gst_pad_has_current_caps (GST_VIDEO_DECODER_SRC_PAD (self)) ||
      acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {
    GstVideoCodecState *state;
    OMX_PARAM_PORTDEFINITIONTYPE port_def;
    GstVideoFormat format;

    GST_DEBUG_OBJECT (self, "Port settings have changed, updating caps");

    /* Reallocate all buffers */
    if (acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE
        && gst_omx_port_is_enabled (port)) {
      gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self),
          gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
              gst_structure_new_empty("ReleaseLastBuffer")));

      err = gst_omx_port_set_enabled (port, FALSE);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      err = gst_omx_port_wait_buffers_released (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      err = gst_omx_video_dec_deallocate_output_buffers (self);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      err = gst_omx_port_wait_enabled (port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
    }

    if (acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {
      /* We have the possibility to reconfigure everything now */
      err = gst_omx_video_dec_reconfigure_output_port (self);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
    } else {
      /* Just update caps */
      GST_VIDEO_DECODER_STREAM_LOCK (self);

      gst_omx_port_get_port_definition (port, &port_def);
      g_assert (port_def.format.video.eCompressionFormat ==
          OMX_VIDEO_CodingUnused);

      switch (port_def.format.video.eColorFormat) {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420PackedPlanar:
          GST_DEBUG_OBJECT (self, "Output is I420 (%d)",
              port_def.format.video.eColorFormat);
          format = GST_VIDEO_FORMAT_I420;
          break;
        case OMX_COLOR_FormatYUV420SemiPlanar:
          GST_DEBUG_OBJECT (self, "Output is NV12 (%d)",
              port_def.format.video.eColorFormat);
          format = GST_VIDEO_FORMAT_NV12;
          break;
        default:
          GST_ERROR_OBJECT (self, "Unsupported color format: %d",
              port_def.format.video.eColorFormat);
          if (buf)
            gst_omx_port_release_buffer (port, buf);
          GST_VIDEO_DECODER_STREAM_UNLOCK (self);
          goto caps_failed;
          break;
      }

      GST_DEBUG_OBJECT (self,
          "Setting output state: format %s, width %u, height %u",
          gst_video_format_to_string (format),
          (guint) port_def.format.video.nFrameWidth,
          (guint) port_def.format.video.nFrameHeight);

      state = gst_video_decoder_set_output_state (GST_VIDEO_DECODER (self),
          format, port_def.format.video.nFrameWidth,
          port_def.format.video.nFrameHeight, self->input_state);

      /* Take framerate and pixel-aspect-ratio from sinkpad caps */

      if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (self))) {
        if (buf)
          gst_omx_port_release_buffer (port, buf);
        gst_video_codec_state_unref (state);
        goto caps_failed;
      }

      gst_video_codec_state_unref (state);

      GST_VIDEO_DECODER_STREAM_UNLOCK (self);
    }

    /* Now get a buffer */
    if (acq_return != GST_OMX_ACQUIRE_BUFFER_OK) {
      return;
    }
  }

  g_assert (acq_return == GST_OMX_ACQUIRE_BUFFER_OK);

  /* This prevents a deadlock between the srcpad stream
   * lock and the videocodec stream lock, if ::reset()
   * is called at the wrong time
   */
  if (gst_omx_port_is_flushing (port)) {
    GST_DEBUG_OBJECT (self, "Flushing");
    gst_omx_port_release_buffer (port, buf);
    goto flushing;
  }

  GST_DEBUG_OBJECT (self, "Handling buffer: 0x%08x %" G_GUINT64_FORMAT,
      (guint) buf->omx_buf->nFlags, (guint64) buf->omx_buf->nTimeStamp);

  GST_VIDEO_DECODER_STREAM_LOCK (self);
  frame = _find_nearest_frame (self, buf);

  if (frame
      && (deadline = gst_video_decoder_get_max_decode_time
          (GST_VIDEO_DECODER (self), frame)) < 0) {
    GST_WARNING_OBJECT (self,
        "Frame is too late, dropping (deadline %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (-deadline));
    flow_ret = gst_video_decoder_drop_frame (GST_VIDEO_DECODER (self), frame);
    frame = NULL;
  } else if (!frame && (buf->omx_buf->nFilledLen > 0 || buf->eglimage)) {
    GstBuffer *outbuf;

    /* This sometimes happens at EOS or if the input is not properly framed,
     * let's handle it gracefully by allocating a new buffer for the current
     * caps and filling it
     */

    GST_ERROR_OBJECT (self, "No corresponding frame found");

    if (self->out_port_pool) {
      gint i, n;
      GstBufferPoolAcquireParams params = { 0, };

      n = port->buffers->len;
      for (i = 0; i < n; i++) {
        GstOMXBuffer *tmp = g_ptr_array_index (port->buffers, i);

        if (tmp == buf)
          break;
      }
      g_assert (i != n);

      GST_OMX_BUFFER_POOL (self->out_port_pool)->current_buffer_index = i;
      flow_ret =
          gst_buffer_pool_acquire_buffer (self->out_port_pool, &outbuf,
          &params);
      if (flow_ret != GST_FLOW_OK) {
        gst_omx_port_release_buffer (port, buf);
        goto invalid_buffer;
      }
      buf = NULL;
    } else {
      outbuf =
          gst_video_decoder_allocate_output_buffer (GST_VIDEO_DECODER (self));
      if (!gst_omx_video_dec_fill_buffer (self, buf, outbuf)) {
        gst_buffer_unref (outbuf);
        gst_omx_port_release_buffer (port, buf);
        goto invalid_buffer;
      }
    }

    flow_ret = gst_pad_push (GST_VIDEO_DECODER_SRC_PAD (self), outbuf);
  } else if (buf->omx_buf->nFilledLen > 0 || buf->eglimage) {
    if (self->out_port_pool) {
      gint i, n;
      GstBufferPoolAcquireParams params = { 0, };

      n = port->buffers->len;
      for (i = 0; i < n; i++) {
        GstOMXBuffer *tmp = g_ptr_array_index (port->buffers, i);

        if (tmp == buf)
          break;
      }
      g_assert (i != n);

      GST_OMX_BUFFER_POOL (self->out_port_pool)->current_buffer_index = i;
      flow_ret =
          gst_buffer_pool_acquire_buffer (self->out_port_pool,
          &frame->output_buffer, &params);
      if (flow_ret != GST_FLOW_OK) {
        flow_ret =
            gst_video_decoder_drop_frame (GST_VIDEO_DECODER (self), frame);
        frame = NULL;
        gst_omx_port_release_buffer (port, buf);
        goto invalid_buffer;
      }
      flow_ret =
          gst_video_decoder_finish_frame (GST_VIDEO_DECODER (self), frame);
      frame = NULL;
      buf = NULL;
    } else {
      if ((flow_ret =
              gst_video_decoder_allocate_output_frame (GST_VIDEO_DECODER
                  (self), frame)) == GST_FLOW_OK) {
        /* FIXME: This currently happens because of a race condition too.
         * We first need to reconfigure the output port and then the input
         * port if both need reconfiguration.
         */
        if (!gst_omx_video_dec_fill_buffer (self, buf, frame->output_buffer)) {
          gst_buffer_replace (&frame->output_buffer, NULL);
          flow_ret =
              gst_video_decoder_drop_frame (GST_VIDEO_DECODER (self), frame);
          frame = NULL;
          gst_omx_port_release_buffer (port, buf);
          goto invalid_buffer;
        }
        flow_ret =
            gst_video_decoder_finish_frame (GST_VIDEO_DECODER (self), frame);
        frame = NULL;
      }
    }
  } else if (frame != NULL) {
    flow_ret = gst_video_decoder_drop_frame (GST_VIDEO_DECODER (self), frame);
    frame = NULL;
  }

  GST_DEBUG_OBJECT (self, "Read frame from component");

  GST_DEBUG_OBJECT (self, "Finished frame: %s", gst_flow_get_name (flow_ret));

  if (buf) {
    err = gst_omx_port_release_buffer (port, buf);
    if (err != OMX_ErrorNone)
      goto release_error;
  }

  self->downstream_flow_ret = flow_ret;

  if (flow_ret != GST_FLOW_OK)
    goto flow_error;

  GST_VIDEO_DECODER_STREAM_UNLOCK (self);

  return;

component_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("OpenMAX component in error state %s (0x%08x)",
            gst_omx_component_get_last_error_string (self->dec),
            gst_omx_component_get_last_error (self->dec)));
    gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    self->started = FALSE;
    return;
  }

flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
    gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
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
      gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
    } else {
      GST_DEBUG_OBJECT (self, "Component signalled EOS");
      flow_ret = GST_FLOW_EOS;
    }
    g_mutex_unlock (&self->drain_lock);

    GST_VIDEO_DECODER_STREAM_LOCK (self);
    self->downstream_flow_ret = flow_ret;

    /* Here we fallback and pause the task for the EOS case */
    if (flow_ret != GST_FLOW_OK)
      goto flow_error;

    GST_VIDEO_DECODER_STREAM_UNLOCK (self);

    return;
  }

flow_error:
  {
    if (flow_ret == GST_FLOW_EOS) {
      GST_DEBUG_OBJECT (self, "EOS");

      gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
    } else if (flow_ret == GST_FLOW_NOT_LINKED || flow_ret < GST_FLOW_EOS) {
      GST_ELEMENT_ERROR (self, STREAM, FAILED,
          ("Internal data stream error."), ("stream stopped, reason %s",
              gst_flow_get_name (flow_ret)));

      gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
    }
    self->started = FALSE;
    GST_VIDEO_DECODER_STREAM_UNLOCK (self);
    return;
  }

reconfigure_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Unable to reconfigure output port"));
    gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    self->started = FALSE;
    return;
  }

invalid_buffer:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Invalid sized input buffer"));
    gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_NOT_NEGOTIATED;
    self->started = FALSE;
    GST_VIDEO_DECODER_STREAM_UNLOCK (self);
    return;
  }

caps_failed:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL), ("Failed to set caps"));
    gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
    GST_VIDEO_DECODER_STREAM_UNLOCK (self);
    self->downstream_flow_ret = GST_FLOW_NOT_NEGOTIATED;
    self->started = FALSE;
    return;
  }
release_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Failed to relase output buffer to component: %s (0x%08x)",
            gst_omx_error_to_string (err), err));
    gst_pad_push_event (GST_VIDEO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    self->started = FALSE;
    GST_VIDEO_DECODER_STREAM_UNLOCK (self);
    return;
  }
}

static gboolean
gst_omx_video_dec_start (GstVideoDecoder * decoder)
{
  GstOMXVideoDec *self;

  self = GST_OMX_VIDEO_DEC (decoder);

  self->last_upstream_ts = 0;
  self->eos = FALSE;
  self->downstream_flow_ret = GST_FLOW_OK;

  return TRUE;
}

static gboolean
gst_omx_video_dec_stop (GstVideoDecoder * decoder)
{
  GstOMXVideoDec *self;

  self = GST_OMX_VIDEO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Stopping decoder");

  gst_omx_port_set_flushing (self->dec_in_port, 5 * GST_SECOND, TRUE);
  gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, TRUE);

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  gst_omx_port_set_flushing (self->egl_in_port, 5 * GST_SECOND, TRUE);
  gst_omx_port_set_flushing (self->egl_out_port, 5 * GST_SECOND, TRUE);
#endif

  gst_pad_stop_task (GST_VIDEO_DECODER_SRC_PAD (decoder));

  if (gst_omx_component_get_state (self->dec, 0) > OMX_StateIdle)
    gst_omx_component_set_state (self->dec, OMX_StateIdle);
#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  if (gst_omx_component_get_state (self->egl_render, 0) > OMX_StateIdle)
    gst_omx_component_set_state (self->egl_render, OMX_StateIdle);
#endif

  self->downstream_flow_ret = GST_FLOW_FLUSHING;
  self->started = FALSE;
  self->eos = FALSE;

  g_mutex_lock (&self->drain_lock);
  self->draining = FALSE;
  g_cond_broadcast (&self->drain_cond);
  g_mutex_unlock (&self->drain_lock);

  gst_omx_component_get_state (self->dec, 5 * GST_SECOND);
#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  gst_omx_component_get_state (self->egl_render, 1 * GST_SECOND);
#endif

  gst_buffer_replace (&self->codec_data, NULL);

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = NULL;

  GST_DEBUG_OBJECT (self, "Stopped decoder");

  return TRUE;
}

typedef struct
{
  GstVideoFormat format;
  OMX_COLOR_FORMATTYPE type;
} VideoNegotiationMap;

static void
video_negotiation_map_free (VideoNegotiationMap * m)
{
  g_slice_free (VideoNegotiationMap, m);
}

static GList *
gst_omx_video_dec_get_supported_colorformats (GstOMXVideoDec * self)
{
  GstOMXComponent *comp;
  GstOMXPort *port;
  GstVideoCodecState *state = self->input_state;
  OMX_VIDEO_PARAM_PORTFORMATTYPE param;
  OMX_ERRORTYPE err;
  GList *negotiation_map = NULL;
  gint old_index;
  VideoNegotiationMap *m;

  port = self->dec_out_port;
  comp = self->dec;

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = port->index;
  param.nIndex = 0;
  if (!state || state->info.fps_n == 0)
    param.xFramerate = 0;
  else
    param.xFramerate = (state->info.fps_n << 16) / (state->info.fps_d);

  old_index = -1;
  do {
    err =
        gst_omx_component_get_parameter (comp,
        OMX_IndexParamVideoPortFormat, &param);

    /* FIXME: Workaround for Bellagio that simply always
     * returns the same value regardless of nIndex and
     * never returns OMX_ErrorNoMore
     */
    if (old_index == param.nIndex)
      break;

    if (err == OMX_ErrorNone || err == OMX_ErrorNoMore) {
      switch (param.eColorFormat) {

        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420PackedPlanar:
          m = g_slice_new (VideoNegotiationMap);
          m->format = GST_VIDEO_FORMAT_I420;
          m->type = param.eColorFormat;
          negotiation_map = g_list_append (negotiation_map, m);
          GST_DEBUG_OBJECT (self, "Component supports I420 (%d) at index %u",
              param.eColorFormat, (guint) param.nIndex);
          break;
        case OMX_COLOR_FormatYUV420SemiPlanar:
          m = g_slice_new (VideoNegotiationMap);
          m->format = GST_VIDEO_FORMAT_NV12;
          m->type = param.eColorFormat;
          negotiation_map = g_list_append (negotiation_map, m);
          GST_DEBUG_OBJECT (self, "Component supports NV12 (%d) at index %u",
              param.eColorFormat, (guint) param.nIndex);
          break;
        default:
          GST_DEBUG_OBJECT (self,
              "Component supports unsupported color format %d at index %u",
              param.eColorFormat, (guint) param.nIndex);
          break;
      }
    }
    old_index = param.nIndex++;
  } while (err == OMX_ErrorNone);

  return negotiation_map;
}

static gboolean
gst_omx_video_dec_negotiate (GstOMXVideoDec * self)
{
  OMX_VIDEO_PARAM_PORTFORMATTYPE param;
  OMX_ERRORTYPE err;
  GstCaps *comp_supported_caps;
  GList *negotiation_map = NULL, *l;
  GstCaps *templ_caps, *intersection;
  GstVideoFormat format;
  GstStructure *s;
  const gchar *format_str;

  GST_DEBUG_OBJECT (self, "Trying to negotiate a video format with downstream");

  templ_caps = gst_pad_get_pad_template_caps (GST_VIDEO_DECODER_SRC_PAD (self));
  intersection =
      gst_pad_peer_query_caps (GST_VIDEO_DECODER_SRC_PAD (self), templ_caps);
  gst_caps_unref (templ_caps);

  GST_DEBUG_OBJECT (self, "Allowed downstream caps: %" GST_PTR_FORMAT,
      intersection);

  negotiation_map = gst_omx_video_dec_get_supported_colorformats (self);
  comp_supported_caps = gst_caps_new_empty ();
  for (l = negotiation_map; l; l = l->next) {
    VideoNegotiationMap *map = l->data;

    gst_caps_append_structure (comp_supported_caps,
        gst_structure_new ("video/x-raw",
            "format", G_TYPE_STRING,
            gst_video_format_to_string (map->format), NULL));
  }

  if (!gst_caps_is_empty (comp_supported_caps)) {
    GstCaps *tmp;

    tmp = gst_caps_intersect (comp_supported_caps, intersection);
    gst_caps_unref (intersection);
    intersection = tmp;
  }
  gst_caps_unref (comp_supported_caps);

  if (gst_caps_is_empty (intersection)) {
    gst_caps_unref (intersection);
    GST_ERROR_OBJECT (self, "Empty caps");
    g_list_free_full (negotiation_map,
        (GDestroyNotify) video_negotiation_map_free);
    return FALSE;
  }

  intersection = gst_caps_truncate (intersection);
  intersection = gst_caps_fixate (intersection);

  s = gst_caps_get_structure (intersection, 0);
  format_str = gst_structure_get_string (s, "format");
  if (!format_str ||
      (format =
          gst_video_format_from_string (format_str)) ==
      GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Invalid caps: %" GST_PTR_FORMAT, intersection);
    g_list_free_full (negotiation_map,
        (GDestroyNotify) video_negotiation_map_free);
    return FALSE;
  }

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = self->dec_out_port->index;

  for (l = negotiation_map; l; l = l->next) {
    VideoNegotiationMap *m = l->data;

    if (m->format == format) {
      param.eColorFormat = m->type;
      break;
    }
  }

  GST_DEBUG_OBJECT (self, "Negotiating color format %s (%d)", format_str,
      param.eColorFormat);

  /* We must find something here */
  g_assert (l != NULL);
  g_list_free_full (negotiation_map,
      (GDestroyNotify) video_negotiation_map_free);

  err =
      gst_omx_component_set_parameter (self->dec,
      OMX_IndexParamVideoPortFormat, &param);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Failed to set video port format: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
  }

  return (err == OMX_ErrorNone);
}

static gboolean
gst_omx_video_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstOMXVideoDec *self;
  GstOMXVideoDecClass *klass;
  GstVideoInfo *info = &state->info;
  gboolean is_format_change = FALSE;
  gboolean needs_disable = FALSE;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;

  self = GST_OMX_VIDEO_DEC (decoder);
  klass = GST_OMX_VIDEO_DEC_GET_CLASS (decoder);

  GST_DEBUG_OBJECT (self, "Setting new caps %" GST_PTR_FORMAT, state->caps);

  gst_omx_port_get_port_definition (self->dec_in_port, &port_def);

  /* Check if the caps change is a real format change or if only irrelevant
   * parts of the caps have changed or nothing at all.
   */
  is_format_change |= port_def.format.video.nFrameWidth != info->width;
  is_format_change |= port_def.format.video.nFrameHeight != info->height;
  is_format_change |= (port_def.format.video.xFramerate == 0
      && info->fps_n != 0)
      || (port_def.format.video.xFramerate !=
      (info->fps_n << 16) / (info->fps_d));
  is_format_change |= (self->codec_data != state->codec_data);
  if (klass->is_format_change)
    is_format_change |=
        klass->is_format_change (self, self->dec_in_port, state);

  needs_disable =
      gst_omx_component_get_state (self->dec,
      GST_CLOCK_TIME_NONE) != OMX_StateLoaded;
  /* If the component is not in Loaded state and a real format change happens
   * we have to disable the port and re-allocate all buffers. If no real
   * format change happened we can just exit here.
   */
  if (needs_disable && !is_format_change) {
    GST_DEBUG_OBJECT (self,
        "Already running and caps did not change the format");
    if (self->input_state)
      gst_video_codec_state_unref (self->input_state);
    self->input_state = gst_video_codec_state_ref (state);
    return TRUE;
  }

  if (needs_disable && is_format_change) {
#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
    GstOMXPort *out_port =
        self->eglimage ? self->egl_out_port : self->dec_out_port;
#else
    GstOMXPort *out_port = self->dec_out_port;
#endif

    GST_DEBUG_OBJECT (self, "Need to disable and drain decoder");

    gst_omx_video_dec_drain (self, FALSE);
    gst_omx_port_set_flushing (self->dec_in_port, 5 * GST_SECOND, TRUE);
    gst_omx_port_set_flushing (out_port, 5 * GST_SECOND, TRUE);

    /* Wait until the srcpad loop is finished,
     * unlock GST_VIDEO_DECODER_STREAM_LOCK to prevent deadlocks
     * caused by using this lock from inside the loop function */
    GST_VIDEO_DECODER_STREAM_UNLOCK (self);
    gst_pad_stop_task (GST_VIDEO_DECODER_SRC_PAD (decoder));
    GST_VIDEO_DECODER_STREAM_LOCK (self);

    if (klass->cdata.hacks & GST_OMX_HACK_NO_COMPONENT_RECONFIGURE) {
      GST_VIDEO_DECODER_STREAM_UNLOCK (self);
      gst_omx_video_dec_stop (GST_VIDEO_DECODER (self));
      gst_omx_video_dec_close (GST_VIDEO_DECODER (self));
      GST_VIDEO_DECODER_STREAM_LOCK (self);

      if (!gst_omx_video_dec_open (GST_VIDEO_DECODER (self)))
        return FALSE;
      needs_disable = FALSE;
    } else {
#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
      if (self->eglimage) {
        gst_omx_port_set_flushing (self->dec_in_port, 5 * GST_SECOND, TRUE);
        gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, TRUE);
        gst_omx_port_set_flushing (self->egl_in_port, 5 * GST_SECOND, TRUE);
        gst_omx_port_set_flushing (self->egl_out_port, 5 * GST_SECOND, TRUE);
      }
#endif

      if (gst_omx_port_set_enabled (self->dec_in_port, FALSE) != OMX_ErrorNone)
        return FALSE;
      if (gst_omx_port_wait_buffers_released (self->dec_in_port,
              5 * GST_SECOND) != OMX_ErrorNone)
        return FALSE;
      if (gst_omx_port_deallocate_buffers (self->dec_in_port) != OMX_ErrorNone)
        return FALSE;
      if (gst_omx_port_wait_enabled (self->dec_in_port,
              1 * GST_SECOND) != OMX_ErrorNone)
        return FALSE;

#ifndef USE_OMX_TARGET_TEGRA
      if (gst_omx_port_set_enabled (out_port, FALSE) != OMX_ErrorNone)
        return FALSE;
      if (gst_omx_port_wait_buffers_released (out_port,
              1 * GST_SECOND) != OMX_ErrorNone)
        return FALSE;
      if (gst_omx_video_dec_deallocate_output_buffers (self) != OMX_ErrorNone)
        return FALSE;
      if (gst_omx_port_wait_enabled (out_port, 1 * GST_SECOND) != OMX_ErrorNone)
        return FALSE;
#endif

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
      if (self->eglimage) {
        OMX_STATETYPE egl_state;

        egl_state = gst_omx_component_get_state (self->egl_render, 0);
        if (egl_state > OMX_StateLoaded || egl_state == OMX_StateInvalid) {

          if (egl_state > OMX_StateIdle) {
            gst_omx_component_set_state (self->egl_render, OMX_StateIdle);
            gst_omx_component_set_state (self->dec, OMX_StateIdle);
            egl_state = gst_omx_component_get_state (self->egl_render,
                5 * GST_SECOND);
            gst_omx_component_get_state (self->dec, 1 * GST_SECOND);
          }
          gst_omx_component_set_state (self->egl_render, OMX_StateLoaded);
          gst_omx_component_set_state (self->dec, OMX_StateLoaded);

          gst_omx_component_close_tunnel (self->dec, self->dec_out_port,
              self->egl_render, self->egl_in_port);

          if (egl_state > OMX_StateLoaded) {
            gst_omx_component_get_state (self->egl_render, 5 * GST_SECOND);
          }

          gst_omx_component_set_state (self->dec, OMX_StateIdle);

          gst_omx_component_set_state (self->dec, OMX_StateExecuting);
          gst_omx_component_get_state (self->dec, GST_CLOCK_TIME_NONE);
        }
        self->eglimage = FALSE;
      }
#endif
    }
    if (self->input_state)
      gst_video_codec_state_unref (self->input_state);
    self->input_state = NULL;

    GST_DEBUG_OBJECT (self, "Decoder drained and disabled");
  }

  port_def.format.video.nFrameWidth = info->width;
  port_def.format.video.nFrameHeight = info->height;
  if (info->fps_n == 0)
    port_def.format.video.xFramerate = 0;
  else
    port_def.format.video.xFramerate = (info->fps_n << 16) / (info->fps_d);

  GST_DEBUG_OBJECT (self, "Setting inport port definition");

  if (gst_omx_port_update_port_definition (self->dec_in_port,
          &port_def) != OMX_ErrorNone)
    return FALSE;

  if (klass->set_format) {
    if (!klass->set_format (self, self->dec_in_port, state)) {
      GST_ERROR_OBJECT (self, "Subclass failed to set the new format");
      return FALSE;
    }
  }

  GST_DEBUG_OBJECT (self, "Updating outport port definition");
  if (gst_omx_port_update_port_definition (self->dec_out_port,
          NULL) != OMX_ErrorNone)
    return FALSE;

  gst_buffer_replace (&self->codec_data, state->codec_data);
  self->input_state = gst_video_codec_state_ref (state);

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
    if (!gst_omx_video_dec_negotiate (self))
      GST_LOG_OBJECT (self, "Negotiation failed, will get output format later");

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
  gst_pad_start_task (GST_VIDEO_DECODER_SRC_PAD (self),
      (GstTaskFunction) gst_omx_video_dec_loop, decoder, NULL);

  return TRUE;
}

static gboolean
gst_omx_video_dec_reset (GstVideoDecoder * decoder, gboolean hard)
{
  GstOMXVideoDec *self;

  self = GST_OMX_VIDEO_DEC (decoder);

  /* FIXME: Handle different values of hard */

  GST_DEBUG_OBJECT (self, "Resetting decoder");

  gst_omx_port_set_flushing (self->dec_in_port, 5 * GST_SECOND, TRUE);
  gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, TRUE);

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  gst_omx_port_set_flushing (self->egl_in_port, 5 * GST_SECOND, TRUE);
  gst_omx_port_set_flushing (self->egl_out_port, 5 * GST_SECOND, TRUE);
#endif

  /* Wait until the srcpad loop is finished,
   * unlock GST_VIDEO_DECODER_STREAM_LOCK to prevent deadlocks
   * caused by using this lock from inside the loop function */
  GST_VIDEO_DECODER_STREAM_UNLOCK (self);
  GST_PAD_STREAM_LOCK (GST_VIDEO_DECODER_SRC_PAD (self));
  GST_PAD_STREAM_UNLOCK (GST_VIDEO_DECODER_SRC_PAD (self));
  GST_VIDEO_DECODER_STREAM_LOCK (self);

  gst_omx_port_set_flushing (self->dec_in_port, 5 * GST_SECOND, FALSE);
  gst_omx_port_set_flushing (self->dec_out_port, 5 * GST_SECOND, FALSE);
  gst_omx_port_populate (self->dec_out_port);

#if defined (USE_OMX_TARGET_RPI) && defined (HAVE_GST_GL)
  gst_omx_port_set_flushing (self->egl_in_port, 5 * GST_SECOND, FALSE);
  gst_omx_port_set_flushing (self->egl_out_port, 5 * GST_SECOND, FALSE);
#endif

  /* Start the srcpad loop again */
  self->last_upstream_ts = 0;
  self->eos = FALSE;
  self->downstream_flow_ret = GST_FLOW_OK;
  gst_pad_start_task (GST_VIDEO_DECODER_SRC_PAD (self),
      (GstTaskFunction) gst_omx_video_dec_loop, decoder, NULL);

  GST_DEBUG_OBJECT (self, "Reset decoder");

  return TRUE;
}

static GstFlowReturn
gst_omx_video_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstOMXAcquireBufferReturn acq_ret = GST_OMX_ACQUIRE_BUFFER_ERROR;
  GstOMXVideoDec *self;
  GstOMXVideoDecClass *klass;
  GstOMXPort *port;
  GstOMXBuffer *buf;
  GstBuffer *codec_data = NULL;
  guint offset = 0, size;
  GstClockTime timestamp, duration;
  OMX_ERRORTYPE err;

  self = GST_OMX_VIDEO_DEC (decoder);
  klass = GST_OMX_VIDEO_DEC_GET_CLASS (self);

  GST_DEBUG_OBJECT (self, "Handling frame");

  if (self->eos) {
    GST_WARNING_OBJECT (self, "Got frame after EOS");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_EOS;
  }

#ifndef USE_OMX_TARGET_TEGRA
  if (!self->started && !GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame)) {
    gst_video_decoder_drop_frame (GST_VIDEO_DECODER (self), frame);
    return GST_FLOW_OK;
  }
#endif

  timestamp = frame->pts;
  duration = frame->duration;

  if (self->downstream_flow_ret != GST_FLOW_OK) {
    gst_video_codec_frame_unref (frame);
    return self->downstream_flow_ret;
  }

  if (klass->prepare_frame) {
    GstFlowReturn ret;

    ret = klass->prepare_frame (self, frame);
    if (ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "Preparing frame failed: %s",
          gst_flow_get_name (ret));
      gst_video_codec_frame_unref (frame);
      return ret;
    }
  }

  port = self->dec_in_port;

  size = gst_buffer_get_size (frame->input_buffer);
  while (offset < size) {
    /* Make sure to release the base class stream lock, otherwise
     * _loop() can't call _finish_frame() and we might block forever
     * because no input buffers are released */
    GST_VIDEO_DECODER_STREAM_UNLOCK (self);
    acq_ret = gst_omx_port_acquire_buffer (port, &buf);

    if (acq_ret == GST_OMX_ACQUIRE_BUFFER_ERROR) {
      GST_VIDEO_DECODER_STREAM_LOCK (self);
      goto component_error;
    } else if (acq_ret == GST_OMX_ACQUIRE_BUFFER_FLUSHING) {
      GST_VIDEO_DECODER_STREAM_LOCK (self);
      goto flushing;
    } else if (acq_ret == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {
      /* Reallocate all buffers */
      err = gst_omx_port_set_enabled (port, FALSE);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_buffers_released (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_deallocate_buffers (port);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_enabled (port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_set_enabled (port, TRUE);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_allocate_buffers (port);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_enabled (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_mark_reconfigured (port);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_DECODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      /* Now get a new buffer and fill it */
      GST_VIDEO_DECODER_STREAM_LOCK (self);
      continue;
    }
    GST_VIDEO_DECODER_STREAM_LOCK (self);

    g_assert (acq_ret == GST_OMX_ACQUIRE_BUFFER_OK && buf != NULL);

    if (buf->omx_buf->nAllocLen - buf->omx_buf->nOffset <= 0) {
      gst_omx_port_release_buffer (port, buf);
      goto full_buffer;
    }

    if (self->downstream_flow_ret != GST_FLOW_OK) {
      gst_omx_port_release_buffer (port, buf);
      goto flow_error;
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
    gst_buffer_extract (frame->input_buffer, offset,
        buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
        buf->omx_buf->nFilledLen);

    if (timestamp != GST_CLOCK_TIME_NONE) {
      buf->omx_buf->nTimeStamp =
          gst_util_uint64_scale (timestamp, OMX_TICKS_PER_SECOND, GST_SECOND);
      self->last_upstream_ts = timestamp;
    } else if (duration != GST_CLOCK_TIME_NONE) {
      buf->omx_buf->nTimeStamp = self->last_upstream_ts + duration;
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

    if (offset == 0) {
      BufferIdentification *id = g_slice_new0 (BufferIdentification);

      if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame))
        buf->omx_buf->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

      id->timestamp = buf->omx_buf->nTimeStamp;
      gst_video_codec_frame_set_user_data (frame, id,
          (GDestroyNotify) buffer_identification_free);
    }

    /* TODO: Set flags
     *   - OMX_BUFFERFLAG_DECODEONLY for buffers that are outside
     *     the segment
     */

    offset += buf->omx_buf->nFilledLen;

    if (offset == size)
      buf->omx_buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

    self->started = TRUE;
    err = gst_omx_port_release_buffer (port, buf);
    if (err != OMX_ErrorNone)
      goto release_error;
  }

  gst_video_codec_frame_unref (frame);

  GST_DEBUG_OBJECT (self, "Passed frame to component");

  return self->downstream_flow_ret;

full_buffer:
  {
    gst_video_codec_frame_unref (frame);
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("Got OpenMAX buffer with no free space (%p, %u/%u)", buf,
            (guint) buf->omx_buf->nOffset, (guint) buf->omx_buf->nAllocLen));
    return GST_FLOW_ERROR;
  }

flow_error:
  {
    gst_video_codec_frame_unref (frame);

    return self->downstream_flow_ret;
  }

too_large_codec_data:
  {
    gst_video_codec_frame_unref (frame);
    GST_ELEMENT_ERROR (self, STREAM, FORMAT, (NULL),
        ("codec_data larger than supported by OpenMAX port (%zu > %u)",
            gst_buffer_get_size (codec_data),
            (guint) self->dec_in_port->port_def.nBufferSize));
    return GST_FLOW_ERROR;
  }

component_error:
  {
    gst_video_codec_frame_unref (frame);
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("OpenMAX component in error state %s (0x%08x)",
            gst_omx_component_get_last_error_string (self->dec),
            gst_omx_component_get_last_error (self->dec)));
    return GST_FLOW_ERROR;
  }

flushing:
  {
    gst_video_codec_frame_unref (frame);
    GST_DEBUG_OBJECT (self, "Flushing -- returning FLUSHING");
    return GST_FLOW_FLUSHING;
  }
reconfigure_error:
  {
    gst_video_codec_frame_unref (frame);
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Unable to reconfigure input port"));
    return GST_FLOW_ERROR;
  }
release_error:
  {
    gst_video_codec_frame_unref (frame);
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Failed to relase input buffer to component: %s (0x%08x)",
            gst_omx_error_to_string (err), err));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_omx_video_dec_finish (GstVideoDecoder * decoder)
{
  GstOMXVideoDec *self;

  self = GST_OMX_VIDEO_DEC (decoder);

  return gst_omx_video_dec_drain (self, TRUE);
}

static GstFlowReturn
gst_omx_video_dec_drain (GstOMXVideoDec * self, gboolean is_eos)
{
  GstOMXVideoDecClass *klass;
  GstOMXBuffer *buf;
  GstOMXAcquireBufferReturn acq_ret;
  OMX_ERRORTYPE err;

  GST_DEBUG_OBJECT (self, "Draining component");

  klass = GST_OMX_VIDEO_DEC_GET_CLASS (self);

  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Component not started yet");
    return GST_FLOW_OK;
  }
  self->started = FALSE;

  /* Don't send EOS buffer twice, this doesn't work */
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

  /* Make sure to release the base class stream lock, otherwise
   * _loop() can't call _finish_frame() and we might block forever
   * because no input buffers are released */
  GST_VIDEO_DECODER_STREAM_UNLOCK (self);

  /* Send an EOS buffer to the component and let the base
   * class drop the EOS event. We will send it later when
   * the EOS buffer arrives on the output port. */
  acq_ret = gst_omx_port_acquire_buffer (self->dec_in_port, &buf);
  if (acq_ret != GST_OMX_ACQUIRE_BUFFER_OK) {
    GST_VIDEO_DECODER_STREAM_LOCK (self);
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
    GST_VIDEO_DECODER_STREAM_LOCK (self);
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
  GST_VIDEO_DECODER_STREAM_LOCK (self);

  self->started = FALSE;

  return GST_FLOW_OK;
}

static gboolean
gst_omx_video_dec_decide_allocation (GstVideoDecoder * bdec, GstQuery * query)
{
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  guint max = 0, min = 0, size;
  GstOMXPort *port;
  GstOMXVideoDec *self = GST_OMX_VIDEO_DEC (bdec);
  OMX_ERRORTYPE err = OMX_ErrorNone;

#if (defined (USE_OMX_TARGET_RPI) || defined (USE_OMX_TARGET_TEGRA)) && defined (HAVE_GST_GL)
  {
    GstCaps *caps;
    gint i, n;
    GstVideoInfo info;

    gst_query_parse_allocation (query, &caps, NULL);
    if (caps && gst_video_info_from_caps (&info, caps)
        && info.finfo->format == GST_VIDEO_FORMAT_RGBA) {
      gboolean found = FALSE;
      GstCapsFeatures *feature = gst_caps_get_features (caps, 0);
      /* Prefer an EGLImage allocator if available and we want to use it */
      n = gst_query_get_n_allocation_params (query);
      for (i = 0; i < n; i++) {
        GstAllocator *allocator;
        GstAllocationParams params;

        gst_query_parse_nth_allocation_param (query, i, &allocator, &params);
        if (allocator
            && GST_IS_GL_MEMORY_EGL_ALLOCATOR (allocator)) {
          found = TRUE;
          gst_query_set_nth_allocation_param (query, 0, allocator, &params);
          while (gst_query_get_n_allocation_params (query) > 1)
            gst_query_remove_nth_allocation_param (query, 1);
          break;
        }
      }

      /* if try to negotiate with caps feature memory:GLMemory
       * and if allocator is not of type memory EGLImage then fails */
      if (feature
          && gst_caps_features_contains (feature,
              GST_CAPS_FEATURE_MEMORY_GL_MEMORY) && !found) {
        return FALSE;
      }
    }
  }
#endif

  if (!GST_VIDEO_DECODER_CLASS
      (gst_omx_video_dec_parent_class)->decide_allocation (bdec, query))
    return FALSE;

  g_assert (gst_query_get_n_allocation_pools (query) > 0);
  gst_query_parse_nth_allocation_pool (query, 0, &pool, NULL, NULL, NULL);
  g_assert (pool != NULL);

#ifdef USE_OMX_TARGET_TEGRA
  port = self->dec_out_port;
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, &caps, &size, &min, &max);

  min = MAX (MAX (min, port->port_def.nBufferCountMin), 4);

  if (min != port->port_def.nBufferCountActual) {
    err = gst_omx_port_update_port_definition (port, NULL);
    if (err == OMX_ErrorNone) {
      port->port_def.nBufferCountActual = min;
      err = gst_omx_port_update_port_definition (port, &port->port_def);
    }
    if (err == OMX_ErrorNone) {
      gst_buffer_pool_config_set_params (config, caps, size, min, max);
      if (!gst_buffer_pool_set_config (pool, config)) {
        GST_INFO_OBJECT (self, "Failed to set config on internal pool");
      }
    } else {
      gst_structure_free (config);
    }
  } else {
    gst_structure_free (config);
  }
#endif

  config = gst_buffer_pool_get_config (pool);
  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }

  self->have_affine_transformation_meta =
      gst_query_find_allocation_meta (query,
      GST_VIDEO_AFFINE_TRANSFORMATION_META_API_TYPE, NULL);

  gst_buffer_pool_set_config (pool, config);
  gst_object_unref (pool);

  return TRUE;
}
