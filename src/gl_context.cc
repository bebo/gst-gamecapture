#include "gl_context.h"

#include <windows.h>
#include <gst/video/gstvideoaffinetransformationmeta.h>

static const gfloat identity_matrix[] = {
  1.0, 0.0, 0.0, 0.0,
  0.0, 1.0, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 1.0,
};

static const gfloat from_ndc_matrix[] = {
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 0.5, 0.0,
  0.5, 0.5, 0.5, 1.0,
};

static const gfloat to_ndc_matrix[] = {
  2.0, 0.0, 0.0, 0.0,
  0.0, 2.0, 0.0, 0.0,
  0.0, 0.0, 2.0, 0.0,
  -1.0, -1.0, -1.0, 1.0,
};

static void
gst_gl_multiply_matrix4 (const gfloat * a, const gfloat * b, gfloat * result)
{
  int i, j, k;
  gfloat tmp[16] = { 0.0f };

  if (!a || !b || !result)
    return;

  for (i = 0; i < 4; i++) {     /* column */
    for (j = 0; j < 4; j++) {   /* row */
      for (k = 0; k < 4; k++) {
        tmp[j + (i * 4)] += a[k + (i * 4)] * b[j + (k * 4)];
      }
    }
  }

  for (i = 0; i < 16; i++)
    result[i] = tmp[i];
}

static void gst_gl_get_affine_transformation_meta_as_ndc_ext
    (GstVideoAffineTransformationMeta * meta, gfloat * matrix)
{
  if (!meta) {
    int i;

    for (i = 0; i < 16; i++) {
      matrix[i] = identity_matrix[i];
    }
  } else {
    float tmp[16];

    gst_gl_multiply_matrix4 (to_ndc_matrix, meta->matrix, tmp);
    gst_gl_multiply_matrix4 (tmp, from_ndc_matrix, matrix);
  }
}

void _setup_gl_functions() {
#if 1
  wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC) wglGetProcAddress("wglDXOpenDeviceNV");
  wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC) wglGetProcAddress("wglDXCloseDeviceNV");
  wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC) wglGetProcAddress("wglDXRegisterObjectNV");
  wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC) wglGetProcAddress("wglDXUnregisterObjectNV");
  wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC) wglGetProcAddress("wglDXLockObjectsNV");
  wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC) wglGetProcAddress("wglDXUnlockObjectsNV");
  wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC)
    wglGetProcAddress("wglDXSetResourceShareHandleNV");

  glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) wglGetProcAddress("glGenFramebuffers");
  glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) wglGetProcAddress("glDeleteFramebuffers");
  glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) wglGetProcAddress("glGenRenderbuffers");
  glDeleteRenderbuffers =(PFNGLDELETERENDERBUFFERSPROC) wglGetProcAddress("glDeleteRenderbuffers");
  glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) wglGetProcAddress("glBindFramebuffer");
  glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) wglGetProcAddress("glFramebufferRenderbuffer");
  glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) wglGetProcAddress("glFramebufferTexture2D");
  glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) wglGetProcAddress("glCheckFramebufferStatus");
#else

  wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXOpenDeviceNV");
  wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXCloseDeviceNV");
  wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXRegisterObjectNV");
  wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXUnregisterObjectNV");
  wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC) 
    gst_gl_context_get_proc_address(gl_context, "wglDXLockObjectsNV");
  wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXUnlockObjectsNV");
  wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXSetResourceShareHandleNV");

#endif
}

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

  GST_INFO("VENDOR : %s", glGetString(GL_VENDOR));
  GST_INFO("RENDERER : %s", glGetString(GL_RENDERER));
  GST_INFO("VERSION : %s", glGetString(GL_VERSION));

  _setup_gl_functions();

  gl_open_d3d_device(context, context->native_device);
#if 0
  gl_open_d3d_texture(context, context->native_texture, 
      context->native_shared_surface_handle);
#endif

  context->fbo_bound = FALSE;
  return TRUE;
}

gboolean _init_gst_gl(GLContext* context, GstGLContext* gl_context) {
  GST_INFO("VENDOR : %s", glGetString(GL_VENDOR));
  GST_INFO("RENDERER : %s", glGetString(GL_RENDERER));
  GST_INFO("VERSION : %s", glGetString(GL_VERSION));

  wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXOpenDeviceNV");
  wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXCloseDeviceNV");
  wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXRegisterObjectNV");
  wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXUnregisterObjectNV");
  wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC) 
    gst_gl_context_get_proc_address(gl_context, "wglDXLockObjectsNV");
  wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXUnlockObjectsNV");
  wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC)
    gst_gl_context_get_proc_address(gl_context, "wglDXSetResourceShareHandleNV");

  gl_open_d3d_device(context, context->native_device);
  gl_open_d3d_texture(context, context->native_texture,
      context->native_shared_surface_handle,
      gl_context->gl_vtable);

  const GstGLFuncs* gl = gl_context->gl_vtable;
  gl_bind_texture_to_framebuffer(context, gl);

  return TRUE;
}


GLContext* gl_new_context() {
  GLContext* context = g_new(GLContext, 1);
  context->fbo = NULL;
  context->texture = NULL;
  context->render_context = NULL;
  context->fbo_bound = FALSE;

  _set_format_descriptor(context); 
  return context;
}

void gl_open_d3d_device(GLContext* context, void* d3d_device) {
  GST_INFO("wglDXOpenDeviceNV is NULL: %d", (wglDXOpenDeviceNV == NULL));
  context->device_handle = wglDXOpenDeviceNV(d3d_device);
  GST_INFO("wglDXOpenDeviceNV device_handle: %llu", context->device_handle);
}

void gl_open_d3d_texture(GLContext* context, void* d3d_texture, HANDLE handle, const GstGLFuncs* gl) {
  BOOL success = wglDXSetResourceShareHandleNV(d3d_texture, handle);
  GST_INFO("wglDXSetResourceShareHandleNV success: %d", success != FALSE);

  gl->GenTextures(1, &context->texture);

  context->texture_handle = wglDXRegisterObjectNV(context->device_handle, 
      context->native_texture, context->texture,
      GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);

  gl->BindTexture(GL_TEXTURE_2D, context->texture);
  if (gl->GetError() != 0) GST_ERROR("Failed glBindTexture");


#if 0
  gl->GenerateMipmap(GL_TEXTURE_2D);
  if (gl->GetError() != 0) GST_ERROR("Failed glGenerateMipmap");
#endif

  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (gl->GetError() != 0) GST_ERROR("Failed glTexParameteri1");

  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  if (gl->GetError() != 0) GST_ERROR("Failed glTexParameteri2");

  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  if (gl->GetError() != 0) GST_ERROR("Failed glTexParameteri3");

  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (gl->GetError() != 0) GST_ERROR("Failed glTexParameteri4");

  gl->BindTexture(GL_TEXTURE_2D, 0);
  if (gl->GetError() != 0) GST_ERROR("Failed glBindTexture");
}

void gl_bind_texture_to_framebuffer(GLContext* context, const GstGLFuncs* gl) {
  context->fbo_bound = TRUE;
#if 0
  gl->GenFramebuffers(1, &context->fbo);
  gl->BindFramebuffer(GL_FRAMEBUFFER, context->fbo);

  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D, context->texture, 0);
  if (gl->GetError() != 0) GST_ERROR("Failed glFramebufferTexture2D");

  GLenum status = gl->CheckFramebufferStatus(GL_FRAMEBUFFER);
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

  //static const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
  //gl->DrawBuffers(1, draw_buffers);

  gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
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

gboolean gl_get_frame_cpu(GLContext* context, void* buffer) {
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

  glReadPixels(0, 0, 1280, 720, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

  // SwapBuffers(hdc);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  wglDXUnlockObjectsNV(context->device_handle, 1, &context->texture_handle);

  return TRUE;
}

/* *INDENT-OFF* */
/*  
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f
     */

static const GLfloat vertices[] = {
  -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
  1.0f,  1.0f,  0.0f, 1.0f, 1.0f,
  -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
  1.0f, -1.0f,  0.0f, 1.0f, 0.0f
};

static const GLushort indices[] = { 0, 1, 2, 1, 3, 2 };
/* *INDENT-ON* */
static void
_bind_buffer (GLContext *gl_sink, const GstGLFuncs *gl )
{
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, gl_sink->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, gl_sink->vertex_buffer);

  /* Load the vertex position */
  gl->VertexAttribPointer (gl_sink->attr_position, 3, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (gl_sink->attr_texture, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));

  gl->EnableVertexAttribArray (gl_sink->attr_position);
  gl->EnableVertexAttribArray (gl_sink->attr_texture);
}

static void
_unbind_buffer (GLContext *gl_sink, const GstGLFuncs *gl )
{
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (gl_sink->attr_position);
  gl->DisableVertexAttribArray (gl_sink->attr_texture);
}

gboolean gl_get_frame_gpu(GLContext* context, GstGLContext* gst_gl_context, GstBuffer *buffer) {
  const GstGLFuncs* gl = gst_gl_context->gl_vtable;

  if (!context->fbo_bound) {
    _init_gst_gl(context, gst_gl_context);
    GST_INFO("CONTEXT->TEXTURE > %llu", context->texture);

    // init shader shit
    GError *error = NULL;
    GstGLSLStage *frag_stage, *vert_stage;
    vert_stage = gst_glsl_stage_new_default_vertex (gst_gl_context);
    frag_stage = gst_glsl_stage_new_default_fragment (gst_gl_context);

    context->display_shader =
      gst_gl_shader_new_link_with_stages (gst_gl_context, &error,
          vert_stage, frag_stage, NULL);

    context->attr_position =
      gst_gl_shader_get_attribute_location (context->display_shader,
          "a_position");
    context->attr_texture =
      gst_gl_shader_get_attribute_location (context->display_shader,
          "a_texcoord");

    if (gl->GenVertexArrays) {
      gl->GenVertexArrays (1, &context->vao);
      gl->BindVertexArray (context->vao);
    }

    if (!context->vertex_buffer) {
      gl->GenBuffers (1, &context->vertex_buffer);
      gl->BindBuffer (GL_ARRAY_BUFFER, context->vertex_buffer);
      gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices,
          GL_STATIC_DRAW);
    }

    if (!context->vbo_indices) {
      gl->GenBuffers (1, &context->vbo_indices);
      gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, context->vbo_indices);
      gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
          GL_STATIC_DRAW);
    }

    if (gl->GenVertexArrays) {
      _bind_buffer (context, gl);
      gl->BindVertexArray (0);
    }

    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
    gl->BindBuffer (GL_ARRAY_BUFFER, 0);
    
  }


  // cleaning old resources
  gst_gl_context_clear_shader (gst_gl_context);
  gl->BindTexture (GL_TEXTURE_2D, 0);

  // start new shits
  gl->ClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  gl->Clear(GL_COLOR_BUFFER_BIT);

  gst_gl_shader_use (context->display_shader);
  
  wglDXLockObjectsNV(context->device_handle, 1, &context->texture_handle);


  if (gl->GenVertexArrays)
      gl->BindVertexArray (context->vao);
  _bind_buffer (context, gl);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture (GL_TEXTURE_2D, context->texture);
  gst_gl_shader_set_uniform_1i (context->display_shader, "tex", 0);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

  gl->BindTexture (GL_TEXTURE_2D, 0);
  gst_gl_context_clear_shader (gst_gl_context);

  if (gl->GenVertexArrays)
    gl->BindVertexArray (0);
  _unbind_buffer (context, gl);

  wglDXUnlockObjectsNV(context->device_handle, 1, &context->texture_handle);

  return TRUE;
}
