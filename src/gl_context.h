#ifndef CHOCOBO_GL_CONTEXT_H_
#define CHOCOBO_GL_CONTEXT_H_
#include <d3d11.h>
#include <dxgi.h>

#include <gst/gst.h>
#include <gst/gl/gl.h>

// #include <gst/gl/gstglutils.h>
#include <GL/glext.h>
#include <GL/wglext.h>


#ifdef __cplusplus 
extern "C" {
#endif 


GST_EXPORT GstDebugCategory *gst_chocobopushsrc_debug;
#define GST_CAT_DEFAULT gst_chocobopushsrc_debug

static PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV;
static PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV;
static PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV;
static PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV;
static PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV;
static PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV;
static PFNWGLDXSETRESOURCESHAREHANDLENVPROC wglDXSetResourceShareHandleNV;

#if 1
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
static PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
static PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
#endif


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

  gboolean fbo_bound;

  // shader
  GstGLShader *display_shader;
  GLuint vao;
  GLuint vbo_indices;
  GLuint vertex_buffer;
  GLint  attr_position;
  GLint  attr_texture;
};

GLContext* gl_new_context();
void gl_destroy_context(GLContext* context);

void gl_open_d3d_device(GLContext* context, void* d3d_device);
void gl_open_d3d_texture(GLContext* context, void* d3d_texture, HANDLE handle, const GstGLFuncs* gl);
void gl_bind_texture_to_framebuffer(GLContext* context, const GstGLFuncs* gst_gl_context);

gboolean _init_gl(GLContext* context);
gboolean _init_gst_gl(GLContext* context, GstGLContext* gst_gl_context);
void _set_format_descriptor(GLContext* context);
void _setup_gl_functions();

gboolean gl_ensure_context(GLContext* context);
gboolean gl_get_frame_cpu(GLContext* context, void* buffer);
gboolean gl_get_frame_gpu(GLContext* context, GstGLContext* gst_gl_context, GstBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif /* CHOCOBO_GL_CONTEXT_H_*/
