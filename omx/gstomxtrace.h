/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.

 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef GST_OMX_TRACE_H
#define GST_OMX_TRACE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

int gst_omx_trace_file_open (FILE ** file);
void gst_omx_trace_file_close (FILE * file);
void gst_omx_trace_printf (FILE * file, const char *fmt, ...);

#endif // GST_OMX_TRACE_H
