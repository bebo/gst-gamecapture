#ifndef CHOCOBO_GL_CONTEXT_H_
#define CHOCOBO_GL_CONTEXT_H_

#include <gst/gst.h>

#include <d3d11.h>
#include <dxgi.h>
#include <GL/glew.h>

#ifdef __cplusplus 
extern "C" {
#endif 

GST_EXPORT GstDebugCategory *gst_chocobopushsrc_debug;
#define GST_CAT_DEFAULT gst_chocobopushsrc_debug

typedef struct _GLContext GLContext;
struct _GLContext {
  HANDLE device_handle;
  HANDLE texture_handle;

  GLuint fbo;
  GLuint texture;

  HDC hdc;
  HGLRC render_context;
  HWND window_handle;

  PIXELFORMATDESCRIPTOR format_descriptor;

  void* native_texture;
  void* native_device;
  HANDLE native_shared_surface_handle;
};

GLContext* gl_new_context();
void gl_destroy_context(GLContext* context);

void gl_open_d3d_device(GLContext* context, void* d3d_device);
void gl_open_d3d_texture(GLContext* context, void* d3d_texture, HANDLE handle);
void gl_bind_texture_to_framebuffer(GLContext* context);

gboolean _init_gl(GLContext* context);
void _set_format_descriptor(GLContext* context);

gboolean gl_ensure_context(GLContext* context);
gboolean gl_get_frame(GLContext* context, void* buffer);


#ifdef __cplusplus
}
#endif

#endif /* CHOCOBO_GL_CONTEXT_H_*/
