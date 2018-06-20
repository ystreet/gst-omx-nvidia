/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.

 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <glib.h>
#include <unistd.h>
#include "gstomxtrace.h"

int
gst_omx_trace_file_open (FILE ** TracingFile)
{
  char buf[4096] = { };

  const char *homedir = g_getenv ("HOME");
  if (!homedir)
    homedir = g_get_home_dir ();

  if (homedir == NULL)
    return -1;

  snprintf (buf, sizeof (buf) - 1, "%s/gst_omx_enc_latency_%d.log",
      homedir, (int) getpid ());

  *TracingFile = fopen (buf, "w");

  if (*TracingFile == NULL) {
    return -1;
  }
  return 0;
}

void
gst_omx_trace_file_close (FILE * TracingFile)
{
  if (TracingFile == NULL)
    return;
  fclose (TracingFile);
  TracingFile = NULL;
}

void
gst_omx_trace_printf (FILE * TracingFile, const char *fmt, ...)
{
  va_list ap;

  if (TracingFile != NULL) {
    va_start (ap, fmt);
    vfprintf (TracingFile, fmt, ap);
    fprintf (TracingFile, "\n");
    fflush (TracingFile);
    va_end (ap);
  }
}
