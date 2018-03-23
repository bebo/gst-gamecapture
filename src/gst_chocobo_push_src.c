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
#include <stdbool.h>

#define ELEMENT_NAME  "gamecapture"
#define SUPPORTED_GL_APIS (GST_GL_API_OPENGL3)
// GST_GL_API_OPENGL | GST_GL_API_GLES2

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720
#define DEFAULT_FPS 30
#define UNITS 10000000

extern bool load_graphics_offsets(bool is32bit);
char* dll_inject_path = NULL;

enum
{
  PROP_0,
  PROP_CLASS_NAME,
  PROP_WINDOW_NAME,
  PROP_INJECT_DLL_PATH,
  PROP_ANTI_CHEAT,
  PROP_LAST,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FPS
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
        "format = (string) RGBA, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE ","
        "texture-target = (string) 2D")
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
/* GstElementClass */
static void gst_chocobopushsrc_set_context(GstElement *element,
    GstContext *context);
static GstStateChangeReturn gst_chocobopushsrc_change_state(GstElement *element,
    GstStateChange transition);
/* GstBaseSrc */
static gboolean gst_chocobopushsrc_decide_allocation(GstBaseSrc *basesrc,
    GstQuery *query);
static gboolean gst_chocobopushsrc_set_caps(GstBaseSrc *bsrc, GstCaps *caps);
static GstCaps *gst_chocobopushsrc_fixate(GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_chocobopushsrc_start(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_stop(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_is_seekable(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_unlock(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_unlock_stop(GstBaseSrc *src);
static gboolean gst_chocobopushsrc_query(GstBaseSrc *bsrc, GstQuery *query);
/* GstPushSrc */
static GstFlowReturn gst_chocobopushsrc_fill(GstPushSrc *src, GstBuffer *buf);

static gboolean _find_local_gl_context(GstChocoboPushSrc *src);
static gboolean _gl_context_init_shader(GstChocoboPushSrc *src);
static void _gl_generate_fbo(GstGLContext *context, GstChocoboPushSrc *src);
static void _gl_init(GstGLContext *context, GstChocoboPushSrc *src);


#define _do_init \
  GST_DEBUG_CATEGORY_INIT (gst_chocobopushsrc_debug, ELEMENT_NAME, 0, "Chocobo Video");

G_DEFINE_TYPE_WITH_CODE(GstChocoboPushSrc, gst_chocobopushsrc, GST_TYPE_PUSH_SRC,
    _do_init);

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

  gstelement_class->set_context = gst_chocobopushsrc_set_context;
  gstelement_class->change_state = gst_chocobopushsrc_change_state;

  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_set_caps);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_is_seekable);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_unlock_stop);
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_stop);
  gstbasesrc_class->fixate = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_fixate);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_query);
  gstbasesrc_class->decide_allocation = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_decide_allocation);

  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR(gst_chocobopushsrc_fill);

  g_object_class_install_property (gobject_class, PROP_CLASS_NAME,
      g_param_spec_string ("class-name", "class-name", "window class name to capture",
        "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)); 

  g_object_class_install_property (gobject_class, PROP_WINDOW_NAME,
      g_param_spec_string ("window-name", "window-name", "window name to capture",
        "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)); 

  g_object_class_install_property (gobject_class, PROP_INJECT_DLL_PATH,
      g_param_spec_string ("inject-dll-path", "inject-dll-path", "path to game capture inject dlls",
        "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)); 

  g_object_class_install_property (gobject_class, PROP_ANTI_CHEAT,
      g_param_spec_boolean ("anti-cheat", "anti-cheat", "path to game capture inject dlls",
        TRUE, G_PARAM_READWRITE)); 

  g_object_class_install_property(gobject_class, PROP_WIDTH,
    g_param_spec_uint("width", "width", "output video width",
       1, 8000, DEFAULT_WIDTH, G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_HEIGHT,
    g_param_spec_uint("height", "height", "output video height",
      1, 8000, DEFAULT_HEIGHT, G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_FPS,
    g_param_spec_uint("fps", "fps", "output video fps",
      1, 240, DEFAULT_FPS, G_PARAM_READWRITE));

  gst_element_class_set_static_metadata(gstelement_class,
      "Chocobo video src", "Src/Video",
      "Chocobo video renderer",
      "Jake Loo <dad@bebo.com>");

  gst_element_class_add_static_pad_template(gstelement_class, &src_template);
}

static void
gst_chocobopushsrc_init(GstChocoboPushSrc *src)
{ 
  src->shtex_handle = 0;
  src->shared_resource = NULL;
  src->game_context = NULL;
  src->game_capture_config = g_new0(GameCaptureConfig, 1);
  src->gc_class_name = g_string_new(NULL);
  src->gc_window_name = g_string_new(NULL);
  src->gc_inject_dll_path = g_string_new(NULL);

  /* we operate in time */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (src), TRUE);
  gst_base_src_set_async (GST_BASE_SRC (src), TRUE);
}

/* GObject Functions */

static void
gst_chocobopushsrc_finalize(GObject *gobject)
{
  GstChocoboPushSrc *src = GST_CHOCOBO(gobject);

  G_OBJECT_CLASS(gst_chocobopushsrc_parent_class)->finalize(gobject);
}

static void
gst_chocobopushsrc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  GstChocoboPushSrc *src = GST_CHOCOBO(object);

  switch (prop_id) {
  case PROP_CLASS_NAME:
    {
      g_string_assign(src->gc_class_name, g_value_get_string(value));
      GST_INFO("class-name: %s", src->gc_class_name->str);
      break;
    }
  case PROP_WINDOW_NAME:
    {
      g_string_assign(src->gc_window_name, g_value_get_string(value));
      GST_INFO("window-name: %s", src->gc_window_name->str);
      break;
    }
  case PROP_INJECT_DLL_PATH:
    {
      dll_inject_path = _strdup(g_value_get_string(value));
      g_string_assign(src->gc_inject_dll_path, g_value_get_string(value));
      GST_INFO("inject-dll-path: %s", dll_inject_path);
      break;
    }
  case PROP_ANTI_CHEAT:
    {
      src->gc_anti_cheat = g_value_get_boolean(value);
      GST_INFO("anti-cheat: %d", src->gc_anti_cheat);
      break;
    }
  case PROP_WIDTH:
  {
    src->width = g_value_get_int(value);
    GST_INFO("width: %d", src->width);
    break;
  }
  case PROP_HEIGHT:
  {
    src->height = g_value_get_int(value);
    GST_INFO("height: %d", src->height);
    break;
  }
  case PROP_FPS:
  {
    src->fps = g_value_get_int(value);
    GST_INFO("fps: %d", src->fps);
    break;
  }
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
    case PROP_CLASS_NAME:
      {
        g_value_set_string(value, src->gc_class_name->str);
        break;
      }
    case PROP_WINDOW_NAME:
      {
        g_value_set_string(value, src->gc_window_name->str);
        break;
      }
    case PROP_INJECT_DLL_PATH:
      {
        g_value_set_string(value, src->gc_inject_dll_path->str);
        break;
      }
    case PROP_ANTI_CHEAT:
      {
        g_value_set_boolean(value, src->gc_anti_cheat);
        break;
      }
    case PROP_WIDTH:
    {
      g_value_set_uint(value, src->width);
      break;
    }
    case PROP_HEIGHT:
    {
      g_value_set_uint(value, src->height);
      break;
    }
    case PROP_FPS:
    {
      g_value_set_uint(value, src->fps);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

/* GstBaseSrcClass Functions */
static gboolean
gst_chocobopushsrc_set_caps(GstBaseSrc *bsrc, GstCaps *caps)
{
  GstChocoboPushSrc *src = GST_CHOCOBO(bsrc);
  if (!gst_video_info_from_caps(&src->out_info, caps)) {
    return FALSE;
  }
  return TRUE;
}

static GstCaps *gst_chocobopushsrc_fixate(GstBaseSrc *bsrc, GstCaps *caps) 
{
  GstStructure *structure;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", DEFAULT_WIDTH);
  gst_structure_fixate_field_nearest_int (structure, "height", DEFAULT_HEIGHT);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", DEFAULT_FPS, 1);

  caps = GST_BASE_SRC_CLASS (gst_chocobopushsrc_parent_class)->fixate (bsrc, caps);

  return caps;
}

static gboolean
gst_chocobopushsrc_start(GstBaseSrc *bsrc)
{
  GstChocoboPushSrc *src = GST_CHOCOBO(bsrc);

  GST_DEBUG_OBJECT(bsrc, "Start() called");

  bool s32b = load_graphics_offsets(true);
  bool s64b = load_graphics_offsets(false);
  GST_INFO("load graphics offsets - 32bits: %d, 64bits: %d", 
      s32b, s64b);

  if (!gst_gl_ensure_element_data (src, &src->display, &src->other_context))
    return FALSE;

  gst_gl_display_filter_gl_api (src->display, SUPPORTED_GL_APIS);

  while (src->game_context == NULL) {
    // TODO set the width, height, fps

    src->game_capture_config->scale_cx = src->width; 
    // GST_VIDEO_INFO_WIDTH(&src->out_info);
    src->game_capture_config->scale_cy = src->height; 
    // GST_VIDEO_INFO_HEIGHT(&src->out_info);
    src->game_capture_config->force_scaling = 1;
    src->game_capture_config->anticheat_hook = src->gc_anti_cheat;
    src->game_capture_config->frame_interval = UNITS / src->fps * 100;
    /*(UNITS / GST_VIDEO_INFO_FPS_N(&src->out_info)) * 100*/

    src->game_context = game_capture_start(&src->game_context,
        src->gc_class_name->str,
        src->gc_window_name->str,
        src->game_capture_config);

    // TODO: instead of hardcode sleep time, we need to listen for _unlock too.
    // sleep 15millis
    // no matter if it's game capture is started or not
    // if game capture is started then we wanna wait 1 frame
    // before we start call init_capture_data, otherwise, it's more likely we get 
    // fails to open shared resource error.
    g_usleep(15000);
  }

  if (!game_capture_init_capture_data(src->game_context)) {
    GST_ERROR("Failed to init capture data!");
  }

  gst_base_src_start_complete(bsrc, GST_FLOW_OK);

  return TRUE;
}

static void 
gst_chocobopushsrc_gl_stop(GstGLContext* context,
    GstChocoboPushSrc* src) {
  if (src->fbo) {
    gst_object_unref (src->fbo);
  }
  src->fbo = NULL;

  if (src->shared_resource) {
    free_shared_resource(context, src->shared_resource);
    src->shared_resource = NULL;
  }

  if (src->game_context) {
    game_capture_stop(src->game_context);
    src->game_context = NULL;
  }
}

static gboolean
gst_chocobopushsrc_stop(GstBaseSrc *bsrc)
{
  GstChocoboPushSrc *src = GST_CHOCOBO(bsrc);

  GST_DEBUG_OBJECT(bsrc, "Stop() called");

  if (src->context) {
    gst_gl_context_thread_add(src->context,
        (GstGLContextThreadFunc) gst_chocobopushsrc_gl_stop, src);
  }

  if (src->context) {
    gst_object_unref (src->context);
  }

  src->context = NULL;

  return TRUE;
}

static gboolean
gst_chocobopushsrc_query(GstBaseSrc *bsrc, GstQuery *query)
{
  gboolean res = FALSE;
  GstChocoboPushSrc *src = GST_CHOCOBO(bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      {
        if (gst_gl_handle_context_query ((GstElement *) src, query,
              src->display, src->context, src->other_context))
          return TRUE;
        break;
      }
    case GST_QUERY_CONVERT:
      {
        GstFormat src_fmt, dest_fmt;
        gint64 src_val, dest_val;

        gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
        res =
          gst_video_info_convert (&src->out_info, src_fmt, src_val, dest_fmt,
              &dest_val);
        gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);

        return res;
      }
    default:
      break;
  }

  return GST_BASE_SRC_CLASS (gst_chocobopushsrc_parent_class)->query (bsrc, query);
}


static gboolean
_draw_texture_callback(gpointer stuff)
{
  GstChocoboPushSrc *src = GST_CHOCOBO(stuff);

  src->shared_resource->draw_frame(src->shared_resource,
      src->context);

  return TRUE;
}

static void
_fill_gl(GstGLContext *context, GstChocoboPushSrc *src)
{
  gst_gl_framebuffer_draw_to_texture(src->fbo, src->out_tex,
      _draw_texture_callback, src);
}

static GstFlowReturn
gst_chocobopushsrc_fill(GstPushSrc *psrc, GstBuffer *buffer)
{
  GstChocoboPushSrc *src = GST_CHOCOBO(psrc);

  GstVideoFrame out_frame;

  if (!gst_video_frame_map (&out_frame,
        &src->out_info, buffer,
        GST_MAP_WRITE | GST_MAP_GL)) {

    return GST_FLOW_NOT_NEGOTIATED;
  }

  // if window is gone, we're not retry kay
  if (!game_capture_tick(src->game_context)) {
    return GST_FLOW_EOS;
  }

  src->out_tex = (GstGLMemory *) out_frame.map[0].memory;

  gst_gl_context_thread_add(src->context, (GstGLContextThreadFunc) _fill_gl,
      src);

  gst_video_frame_unmap (&out_frame);

  // TODO collection information for stats

  return GST_FLOW_OK;
}

static void 
gst_chocobopushsrc_set_context(GstElement *element,
    GstContext *context) {
  GstChocoboPushSrc *src = GST_CHOCOBO(element);

  gst_gl_handle_set_context(element, context, &src->display,
      &src->other_context);

  if (src->display)
    gst_gl_display_filter_gl_api (src->display, SUPPORTED_GL_APIS);

  GST_ELEMENT_CLASS(gst_chocobopushsrc_parent_class)->set_context(element, context);
}

static GstStateChangeReturn
gst_chocobopushsrc_change_state(GstElement *element,
    GstStateChange transition) {
  GstChocoboPushSrc *src = GST_CHOCOBO(element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG_OBJECT (src, "changing state: %s => %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_gl_ensure_element_data (element, &src->display,
            &src->other_context))
        return GST_STATE_CHANGE_FAILURE;

      gst_gl_display_filter_gl_api (src->display, SUPPORTED_GL_APIS);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (gst_chocobopushsrc_parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (src->other_context) {
        gst_object_unref (src->other_context);
        src->other_context = NULL;
      }

      if (src->display) {
        gst_object_unref (src->display);
        src->display = NULL;
      }
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
_find_local_gl_context(GstChocoboPushSrc *src)
{
  if (gst_gl_query_local_gl_context (GST_ELEMENT (src), GST_PAD_SRC,
        &src->context))
    return TRUE;
  return FALSE;
}

static gboolean
_gl_context_init_shader(GstChocoboPushSrc *src) 
{
  return gst_gl_context_get_gl_api(src->context);
}

static void 
_gl_generate_fbo(GstGLContext *context, GstChocoboPushSrc *src)
{
  src->fbo = gst_gl_framebuffer_new_with_default_depth(src->context,
      GST_VIDEO_INFO_WIDTH(&src->out_info),
      GST_VIDEO_INFO_HEIGHT(&src->out_info)); 
}

static void 
_gl_init(GstGLContext *context, GstChocoboPushSrc *src)
{
  void* gc_shtex_handle = game_capture_get_shtex_handle(src->game_context);
  if (!gc_shtex_handle) {
    GST_ERROR("unable to accept the fact that gc_shtex_handle is NULL");
  }

  if (src->shtex_handle != gc_shtex_handle) {
    src->shtex_handle = gc_shtex_handle;

    init_shared_resource(src->context, gc_shtex_handle,
        &src->shared_resource);
  }
}

static gboolean
gst_chocobopushsrc_decide_allocation(GstBaseSrc *bsrc, GstQuery *query)
{
  GstChocoboPushSrc *src = GST_CHOCOBO(bsrc);

  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;
  GError *error = NULL;

  if (!gst_gl_ensure_element_data (src, &src->display, &src->other_context))
    return FALSE;

  gst_gl_display_filter_gl_api (src->display, SUPPORTED_GL_APIS);

  _find_local_gl_context (src);

  if (!src->context) {
    GST_OBJECT_LOCK (src->display);
    do {
      if (src->context) {
        gst_object_unref (src->context);
        src->context = NULL;
      }
      /* just get a GL context.  we don't care */
      src->context =
        gst_gl_display_get_gl_context_for_thread (src->display, NULL);
      if (!src->context) {
        if (!gst_gl_display_create_context (src->display, src->other_context,
              &src->context, &error)) {
          GST_OBJECT_UNLOCK (src->display);
          goto context_error;
        }
      }
    } while (!gst_gl_display_add_context (src->display, src->context));
    GST_OBJECT_UNLOCK (src->display);
  }

  if ((gst_gl_context_get_gl_api (src->context) & SUPPORTED_GL_APIS) == 0)
    goto unsupported_gl_api;

  gst_gl_context_thread_add (src->context,
      (GstGLContextThreadFunc) _gl_generate_fbo, src);
  if (!src->fbo)
    goto context_error;

  gst_query_parse_allocation (query, &caps, NULL);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;

    gst_video_info_init (&vinfo);
    gst_video_info_from_caps (&vinfo, caps);
    size = vinfo.size;
    min = max = 0;
    update_pool = FALSE;
  }

  if (!pool || !GST_IS_GL_BUFFER_POOL (pool)) {
    /* can't use this pool */
    if (pool)
      gst_object_unref (pool);
    pool = gst_gl_buffer_pool_new (src->context);
  }
  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (gst_query_find_allocation_meta (query, GST_GL_SYNC_META_API_TYPE, NULL))
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_GL_SYNC_META);
  gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_GL_TEXTURE_UPLOAD_META);

  gst_buffer_pool_set_config (pool, config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  _gl_context_init_shader (src);

  gst_gl_context_thread_add (src->context,
      (GstGLContextThreadFunc) _gl_init, src);

  gst_object_unref (pool);

  return TRUE;

unsupported_gl_api:
  {
    GstGLAPI gl_api = gst_gl_context_get_gl_api (src->context);
    gchar *gl_api_str = gst_gl_api_to_string (gl_api);
    gchar *supported_gl_api_str = gst_gl_api_to_string (SUPPORTED_GL_APIS);
    GST_ELEMENT_ERROR (src, RESOURCE, BUSY,
        ("GL API's not compatible context: %s supported: %s", gl_api_str,
         supported_gl_api_str), (NULL));

    g_free (supported_gl_api_str);
    g_free (gl_api_str);
    return FALSE;
  }
context_error:
  {
    if (error) {
      GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, ("%s", error->message),
          (NULL));
      g_clear_error (&error);
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL), (NULL));
    }
    if (src->context)
      gst_object_unref (src->context);
    src->context = NULL;
    return FALSE;
  }
}

static gboolean
gst_chocobopushsrc_is_seekable(GstBaseSrc *src) 
{
  return FALSE;
}

static gboolean
gst_chocobopushsrc_unlock(GstBaseSrc *src)
{
  GST_ERROR("TODO: implement gst_chocobopushsrc_unlock");
  return TRUE;
}

static gboolean
gst_chocobopushsrc_unlock_stop(GstBaseSrc *src)
{
  GST_ERROR("TODO: implement gst_chocobopushsrc_unlock_stop");
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
