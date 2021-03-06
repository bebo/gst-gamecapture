/*
 * Copyright (C) 2018 Pigs in Flight, Inc.
 * Author: Jake Loo <jake@bebo.com>
 */

#ifndef CHOCOBO_SHARED_RESOURCE_H_
#define CHOCOBO_SHARED_RESOURCE_H_

#include <d3d11.h>
#include <dxgi.h>
#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>

#ifdef __cplusplus
extern "C" {
#endif


GST_EXPORT GstDebugCategory *gst_chocobopushsrc_debug;
#define GST_CAT_DEFAULT gst_chocobopushsrc_debug

typedef struct _SharedResource SharedResource;
struct _SharedResource {
  HANDLE      gl_device_handle;
  HANDLE      gl_texture_handle;
  GLuint      gl_texture;

  void*       d3d_texture;

  // shader
  GstGLShader *display_shader;
  GLuint       vao;
  GLuint       vbo_indices;
  GLuint       vertex_buffer;
  GLint        attr_position;
  GLint        attr_texture;

  void (*draw_frame)(SharedResource* resource, GstGLContext* gl_context);
};

glong init_shared_resource(GstGLContext* gl_context, HANDLE shtex_handle,
    void** resource, gboolean flip);
void free_shared_resource(GstGLContext* gl_context, SharedResource* resource);

#ifdef __cplusplus
}
#endif

#endif /* CHOCOBO_SHARED_RESOURCE_H_*/
