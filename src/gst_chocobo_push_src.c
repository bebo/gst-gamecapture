/* GStreamer
* Copyright (C) 2018 Jake Loo <jake@bebo.com>
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
* Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gst_chocobo_push_src.h"
#include "d3d_chocobo.h"
#include "gl_context.h"

#define ELEMENT_NAME  "chocobopushsrc"

// FIXME
#define WIDTH 1280
#define HEIGHT 720

enum
{
    PROP_0,
    PROP_LAST
};

#define VTS_VIDEO_CAPS GST_VIDEO_CAPS_MAKE ("BGRA")

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(VTS_VIDEO_CAPS)
);

GST_DEBUG_CATEGORY(gst_chocobopushsrc_debug);
#define GST_CAT_DEFAULT gst_chocobopushsrc_debug

/* FWD DECLS */
/* GObject */
static void gst_chocobopushsrc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void gst_chocobopushsrc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);
static void gst_chocobopushsrc_finalize(GObject *gobject);
/* GstBaseSrc */
static gboolean gst_chocobopushsrc_decide_allocation(GstBaseSrc *basesrc,
    GstQuery *query);
static GstCaps *gst_chocobopushsrc_get_caps(GstBaseSrc *basesrc,
    GstCaps *filter);
static gboolean gst_chocobopushsrc_set_caps(GstBaseSrc *bsrc, GstCaps *caps);
static gboolean gst_chocobopushsrc_start(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_stop(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_is_seekable(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_unlock(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_unlock_stop(GstBaseSrc *src);
/* GstPushSrc */
static GstFlowReturn gst_chocobopushsrc_fill(GstPushSrc *src, GstBuffer *buf);

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (gst_chocobopushsrc_debug, ELEMENT_NAME, 0, "Chocobo Video");

G_DEFINE_TYPE_WITH_CODE(GstChocoboPushSrc, gst_chocobopushsrc, GST_TYPE_PUSH_SRC,
    _do_init);

static void *chocobo_context;

static void
gst_chocobopushsrc_class_init(GstChocoboPushSrcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
    GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS(klass);
    GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS(klass);

    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_finalize);
    gobject_class->set_property =
        GST_DEBUG_FUNCPTR(gst_chocobopushsrc_set_property);
    gobject_class->get_property =
        GST_DEBUG_FUNCPTR(gst_chocobopushsrc_get_property);

    gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_get_caps);
    gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_is_seekable);
    gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_unlock);
    gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_unlock_stop);
    gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_start);
    gstbasesrc_class->stop = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_stop);
    // gstbasesrc_class->decide_allocation = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_decide_allocation);

    gstpushsrc_class->fill = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_fill);

#if 0
    gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_get_caps);
    gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_set_caps);
    gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_start);
    gstbasesrc_class->stop = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_stop);
    gstbasesrc_class->propose_allocation =
        GST_DEBUG_FUNCPTR(gst_chocobopushsrc_propose_allocation);
#endif

    /* Add properties */

    gst_element_class_set_static_metadata(gstelement_class,
        "Chocobo video src", "Src/Video",
        "Chocobo video renderer",
        "Jake Loo <dad@bebo.com>");

    gst_element_class_add_static_pad_template(gstelement_class, &src_template);

    g_rec_mutex_init(&klass->lock);
}

static void
gst_chocobopushsrc_init(GstChocoboPushSrc *src)
{
    GST_DEBUG_OBJECT(src, " ");

    g_rec_mutex_init(&src->lock);
}

/* GObject Functions */

static void
gst_chocobopushsrc_finalize(GObject *gobject)
{
    GstChocoboPushSrc *src = GST_CHOCOBO(gobject);

    GST_DEBUG_OBJECT(src, " ");

    gst_object_replace((GstObject **)& src->pool, NULL);
    gst_object_replace((GstObject **)& src->fallback_pool, NULL);

    gst_caps_replace(&src->supported_caps, NULL);

    g_rec_mutex_clear(&src->lock);

    G_OBJECT_CLASS(gst_chocobopushsrc_parent_class)->finalize(gobject);
}

static void
gst_chocobopushsrc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
    GstChocoboPushSrc *src = GST_CHOCOBO(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_chocobopushsrc_get_property(GObject *object, guint prop_id, GValue *value,
    GParamSpec *pspec)
{
    GstChocoboPushSrc *src = GST_CHOCOBO(object);

    switch (prop_id) {
   default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/* GstBaseSrcClass Functions */

static GstCaps *
gst_chocobopushsrc_get_caps(GstBaseSrc *basesrc, GstCaps *filter)
{
    GstChocoboPushSrc *src = GST_CHOCOBO(basesrc);
    GstCaps *caps;

    caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "BGRA",
      "framerate", GST_TYPE_FRACTION, 0, 1,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
      "width", G_TYPE_INT, WIDTH,
      "height", G_TYPE_INT, HEIGHT,
      NULL);

#if 0
    caps = d3d_supported_caps(src);
    if (!caps)
        caps = gst_pad_get_pad_template_caps(GST_VIDEO_src_PAD(src));

    if (caps && filter) {
        GstCaps *isect;
        isect = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(caps);
        caps = isect;
    }
#endif

    return caps;
}

static gboolean
gst_chocobopushsrc_set_caps(GstBaseSrc *bsrc, GstCaps *caps)
{
  return TRUE;
}

static gboolean
gst_chocobopushsrc_start(GstBaseSrc *bsrc)
{
    GstChocoboPushSrc *src = GST_CHOCOBO(bsrc);

    GST_DEBUG_OBJECT(bsrc, "Start() called");

    chocobo_context = d3d11_create_device();

    return TRUE; // d3d_class_init(src);
}

static gboolean
gst_chocobopushsrc_stop(GstBaseSrc *bsrc)
{
    GstChocoboPushSrc *src = GST_CHOCOBO(bsrc);

    GST_DEBUG_OBJECT(bsrc, "Stop() called");

    return TRUE;
}

#define bufsize (WIDTH * HEIGHT * 4 * 1)

static GstFlowReturn
gst_chocobopushsrc_fill(GstPushSrc *src, GstBuffer *buf)
{
  Chocobo *context = (Chocobo*) chocobo_context;

  // TODO remove g_malloc
  void *fake_frame = g_malloc(bufsize);

  while (!gl_get_frame(context->gl_context, fake_frame)) {
    Sleep(10);
  }

  gst_buffer_fill(buf, 0, fake_frame, bufsize);

  g_free(fake_frame);
  return GST_FLOW_OK;
}

static gboolean
gst_chocobopushsrc_decide_allocation(GstBaseSrc *bsrc, GstQuery *query)
{
    GstChocoboPushSrc *src = GST_CHOCOBO(bsrc);
    GstBufferPool *pool;
    GstStructure *config;
    GstCaps *caps;
    guint size;
    gboolean need_pool;

    gst_query_parse_allocation(query, &caps, &need_pool);
    if (!caps) {
        GST_DEBUG_OBJECT(src, "no caps specified");
        return FALSE;
    }

    gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);
    gst_query_add_allocation_meta(query, GST_VIDEO_CROP_META_API_TYPE, NULL);

#ifdef DISABLE_BUFFER_POOL
    return TRUE;
#endif

    /* FIXME re-using buffer pool breaks renegotiation */
    GST_OBJECT_LOCK(src);
    pool = src->pool ? gst_object_ref(src->pool) : NULL;
    GST_OBJECT_UNLOCK(src);

    if (pool) {
        GstCaps *pcaps;

        /* we had a pool, check caps */
        GST_DEBUG_OBJECT(src, "check existing pool caps");
        config = gst_buffer_pool_get_config(pool);
        gst_buffer_pool_config_get_params(config, &pcaps, &size, NULL, NULL);

        if (!gst_caps_is_equal(caps, pcaps)) {
            GST_DEBUG_OBJECT(src, "pool has different caps");
            /* different caps, we can't use this pool */
            gst_object_unref(pool);
            pool = NULL;
        }
        gst_structure_free(config);
    }
    else {
        GstVideoInfo info;

        if (!gst_video_info_from_caps(&info, caps)) {
            GST_ERROR_OBJECT(src, "allocation query has invalid caps %"
                GST_PTR_FORMAT, caps);
            return FALSE;
        }

        /* the normal size of a frame */
        size = info.size;
    }

    if (pool == NULL && need_pool) {
        GST_DEBUG_OBJECT(src, "create new pool");
#if 0
        pool = gst_d3dsurface_buffer_pool_new(src);
#endif

        config = gst_buffer_pool_get_config(pool);
        /* we need at least 2 buffer because we hold on to the last one */
        gst_buffer_pool_config_set_params(config, caps, size, 2, 0);
        if (!gst_buffer_pool_set_config(pool, config)) {
            gst_object_unref(pool);
            GST_ERROR_OBJECT(src, "failed to set pool configuration");
            return FALSE;
        }
    }

    /* we need at least 2 buffer because we hold on to the last one */
    gst_query_add_allocation_pool(query, pool, size, 2, 0);
    if (pool)
        gst_object_unref(pool);

    return TRUE;
}

static gboolean
gst_chocobopushsrc_is_seekable(GstBaseSrc *src) 
{
  return FALSE;
}

static gboolean
gst_chocobopushsrc_unlock(GstBaseSrc *src)
{
  return TRUE;
}

static gboolean
gst_chocobopushsrc_unlock_stop(GstBaseSrc *src)
{
  return TRUE;
}

/* Plugin entry point */
static gboolean
plugin_init(GstPlugin *plugin)
{
    if (!gst_element_register(plugin, ELEMENT_NAME,
        GST_RANK_NONE, GST_TYPE_CHOCOBO))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    chocobo,
    "Chocobo Video Src <3",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
