/*
 * Copyright (C) 2018 Pigs in Flight, Inc.
 * Author: Jake Loo <jake@bebo.com>
 */

#ifndef GST_CHOCOBO_VIDEO_SRC_H_
#define GST_CHOCOBO_VIDEO_SRC_H_

#include <windows.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>
#include <GL/gl.h>
#include "shared_resource.h"
#include "game_capture.h"

G_BEGIN_DECLS

#define GST_TYPE_CHOCOBO                     (gst_chocobopushsrc_get_type())
#define GST_CHOCOBO(obj)                     (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CHOCOBO,GstChocoboPushSrc))
#define GST_CHOCOBO_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CHOCOBO,GstChocoboPushSrcClass))
#define GST_CHOCOBO_GET_CLASS(obj)           (GST_CHOCOBO_CLASS(G_OBJECT_GET_CLASS(obj)))
#define GST_IS_CHOCOBO(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CHOCOBO))
#define GST_IS_CHOCOBO_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CHOCOBO))

typedef struct _GstChocoboPushSrc GstChocoboPushSrc;
typedef struct _GstChocoboPushSrcClass GstChocoboPushSrcClass;

struct _GstChocoboPushSrc
{
    GstPushSrc         src;

    SharedResource     *shared_resource;
    void*               shtex_handle; /*HANDLE*/
    void               *game_context;
    GameCaptureConfig  *game_capture_config;

    GstVideoInfo out_info;
    GstGLFramebuffer *fbo;
    GstGLMemory *out_tex;
    GstBufferPool *pool;
    GstGLDisplay *display;
    GstGLContext *context, *other_context;
    gboolean negotiated;

    gint64 timestamp_offset;
    GstClockTime running_time;
    gint64 n_frames;
    guint64 last_frame_time;
    gboolean got_frame;

    /* properties */
    GString  *gc_class_name;
    GString  *gc_window_name;
    GString  *gc_inject_dll_path;
    gboolean  gc_anti_cheat;
    int width;
    int height;
    int fps;
};

struct _GstChocoboPushSrcClass
{
    GstPushSrcClass parent_class;
};

GType    gst_chocobopushsrc_get_type(void);

G_END_DECLS


#endif /* GST_CHOCOBO_VIDEO_SRC_H_ */
