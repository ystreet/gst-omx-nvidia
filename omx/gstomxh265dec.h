/* GStreamer
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __GST_OMX_H265_DEC_H__
#define __GST_OMX_H265_DEC_H__

#include <gst/gst.h>
#include "gstomxvideodec.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_H265_DEC \
  (gst_omx_h265_dec_get_type())
#define GST_OMX_H265_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_H265_DEC,GstOMXH265Dec))
#define GST_OMX_H265_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_H265_DEC,GstOMXH265DecClass))
#define GST_OMX_H265_DEC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_OMX_H265_DEC,GstOMXH265DecClass))
#define GST_IS_OMX_H265_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_H265_DEC))
#define GST_IS_OMX_H265_DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_H265_DEC))
typedef struct _GstOMXH265Dec GstOMXH265Dec;
typedef struct _GstOMXH265DecClass GstOMXH265DecClass;

struct _GstOMXH265Dec
{
  GstOMXVideoDec parent;
};

struct _GstOMXH265DecClass
{
  GstOMXVideoDecClass parent_class;
};

GType gst_omx_h265_dec_get_type (void);

G_END_DECLS
#endif /* __GST_OMX_H265_DEC_H__ */
