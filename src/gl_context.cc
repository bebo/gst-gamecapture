#include "gl_context.h"

#include <windows.h>
#include <GL/wglew.h>

void _set_format_descriptor(GLContext* context) {
  memset(&context->format_descriptor, 0, sizeof(PIXELFORMATDESCRIPTOR));
  context->format_descriptor.nSize = sizeof(PIXELFORMATDESCRIPTOR);
  context->format_descriptor.nVersion = 1;
  context->format_descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  context->format_descriptor.iPixelType = PFD_TYPE_RGBA;
  context->format_descriptor.cColorBits = 32;
  context->format_descriptor.cDepthBits = 16;
  context->format_descriptor.iLayerType = PFD_MAIN_PLANE;
}

gboolean _init_gl(GLContext* context) {
  context->window_handle = CreateWindowEx(0,
      "STATIC", NULL,
      0,
      0, 0, 0, 0, 0, 0,
      GetModuleHandle(NULL), 0);
  context->hdc = GetDC(context->window_handle);

  SetPixelFormat(context->hdc,
      ChoosePixelFormat(context->hdc, &context->format_descriptor),
      &context->format_descriptor);

  context->render_context = wglCreateContext(context->hdc);

  if (!wglMakeCurrent(context->hdc, context->render_context)) {
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(); 

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    GST_ERROR("wglMakeCurrent fails: %s", lpMsgBuf);
    return FALSE;
  }

  GLenum glew_result = glewInit();
  GST_INFO("glewInit result: %d", (int) glew_result);

  GST_INFO("VENDOR : %s", glGetString(GL_VENDOR));
  GST_INFO("RENDERER : %s", glGetString(GL_RENDERER));
  GST_INFO("VERSION : %s", glGetString(GL_VERSION));

  gl_open_d3d_device(context, context->native_device);
  gl_open_d3d_texture(context, context->native_texture, 
      context->native_shared_surface_handle);

  return glew_result == GLEW_OK;
}

GLContext* gl_new_context() {
  GLContext* context = g_new(GLContext, 1);
  context->fbo = NULL;
  context->texture = NULL;
  context->render_context = NULL;

  _set_format_descriptor(context); 

#if 0
  _init_gl(context);


#endif
  return context;
}

void gl_open_d3d_device(GLContext* context, void* d3d_device) {
  GST_INFO("wglDXOpenDeviceNV is NULL: %d", (wglDXOpenDeviceNV == NULL));
  context->device_handle = wglDXOpenDeviceNV(d3d_device);
  GST_INFO("wglDXOpenDeviceNV device_handle: %llu", context->device_handle);
}

void gl_open_d3d_texture(GLContext* context, void* d3d_texture, HANDLE handle) {
  BOOL success = wglDXSetResourceShareHandleNV(d3d_texture, handle);
  GST_INFO("wglDXSetResourceShareHandleNV success: %d", success != FALSE);

  glGenTextures(1, &context->texture);
  if (glGetError() != 0) GST_ERROR("Failed glGenTextures");

  context->texture_handle = wglDXRegisterObjectNV(context->device_handle, 
      d3d_texture, context->texture,
      GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);

  glBindTexture(GL_TEXTURE_2D, context->texture);
  if (glGetError() != 0) GST_ERROR("Failed glBindTexture");

  //no filtering
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  if (glGetError() != 0) GST_ERROR("Failed glTexParameteri1");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  if (glGetError() != 0) GST_ERROR("Failed glTexParameteri2");

#if 0
  glGenerateMipmap(GL_TEXTURE_2D);
  if (glGetError() != 0) GST_ERROR("Failed glGenerateMipmap");
#endif 

  gl_bind_texture_to_framebuffer(context);
}

void gl_bind_texture_to_framebuffer(GLContext* context) {
  glGenFramebuffers(1, &context->fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, context->fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
      GL_TEXTURE_2D, context->texture, 0);
  if (glGetError() != 0) GST_ERROR("Failed glFramebufferTexture2D");

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status == GL_FRAMEBUFFER_COMPLETE) {
    GST_INFO("framebuffer complete");
  } else {
    GST_INFO("Framebuffer not complete: ");
    const char* errmsg = NULL;
    switch (status) {
      case GL_FRAMEBUFFER_COMPLETE: errmsg = "GL_FRAMEBUFFER_COMPLETE"; break;
      case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"; break;
      case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"; break;
      case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER"; break;
      case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER"; break;
      case GL_FRAMEBUFFER_UNSUPPORTED: errmsg = "GL_FRAMEBUFFER_UNSUPPORTED"; break;
      case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"; break;
      case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS"; break;
    }

    if (errmsg) {
      GST_ERROR("%s", errmsg);
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

gboolean gl_ensure_context(GLContext* context) {
  HGLRC current_context = wglGetCurrentContext();
  if (current_context == NULL ||
      current_context != context->render_context) {

    // if there's already a context, we clean the old one up
    if (context->render_context) {
      wglDeleteContext(context->render_context);
      DeleteDC(context->hdc);
    }

    return _init_gl(context);
  }
  return TRUE;
}

gboolean gl_get_frame(GLContext* context, void* buffer) {
  if (!gl_ensure_context(context)) {
    return FALSE;
  }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  wglDXLockObjectsNV(context->device_handle, 1, &context->texture_handle);

  glBindFramebuffer(GL_FRAMEBUFFER, context->fbo);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, context->texture);

  // TODO remove this
  glReadPixels(0, 0, 1280, 720, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

  // SwapBuffers(hdc);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  wglDXUnlockObjectsNV(context->device_handle, 1, &context->texture_handle);

  return TRUE;
}
