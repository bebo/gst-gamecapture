#include "shared_resource.h"
#include "dxgi/gstdxgidevice.h"

/* *INDENT-OFF* */
static const GLfloat flip_vertices[] = {
  -1.0f,  -1.0f, 0.0f, 0.0f, 1.0f,
  1.0f,  -1.0f,  0.0f, 1.0f, 1.0f,
  -1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
  1.0f, 1.0f,  0.0f, 1.0f, 0.0f
};

static const GLushort indices[] = { 0, 1, 2, 1, 3, 2 };
/* *INDENT-ON* */

static const GLfloat vertices[] = {
  -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
  1.0f,  1.0f,  0.0f, 1.0f, 1.0f,
  -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
  1.0f, -1.0f,  0.0f, 1.0f, 0.0f
};

const static D3D_FEATURE_LEVEL d3d_feature_levels[] =
{
  D3D_FEATURE_LEVEL_11_0,
  D3D_FEATURE_LEVEL_10_1,
  D3D_FEATURE_LEVEL_10_0,
  D3D_FEATURE_LEVEL_9_3,
};

static void _bind_buffer (SharedResource *resource, const GstGLFuncs *gl )
{
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, resource->vbo_indices);
  gl->BindBuffer (GL_ARRAY_BUFFER, resource->vertex_buffer);

  /* Load the vertex position */
  gl->VertexAttribPointer (resource->attr_position, 3, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) 0);

  /* Load the texture coordinate */
  gl->VertexAttribPointer (resource->attr_texture, 2, GL_FLOAT, GL_FALSE,
      5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));

  gl->EnableVertexAttribArray (resource->attr_position);
  gl->EnableVertexAttribArray (resource->attr_texture);
}

static void _unbind_buffer (SharedResource *resource, const GstGLFuncs *gl )
{
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  gl->DisableVertexAttribArray (resource->attr_position);
  gl->DisableVertexAttribArray (resource->attr_texture);
}


static void init_display_shader(SharedResource* resource, GstGLContext* gl_context, gboolean flipped) {
  GError *error = NULL;
  const GstGLFuncs* gl = gl_context->gl_vtable;
  GstGLSLStage* vert_stage = gst_glsl_stage_new_default_vertex (gl_context);
  GstGLSLStage* frag_stage = gst_glsl_stage_new_default_fragment (gl_context);

  resource->display_shader = gst_gl_shader_new_link_with_stages(gl_context, &error,
        vert_stage, frag_stage, NULL);
  resource->attr_position = gst_gl_shader_get_attribute_location(
      resource->display_shader, "a_position");
  resource->attr_texture = gst_gl_shader_get_attribute_location(
      resource->display_shader, "a_texcoord");

  if (gl->GenVertexArrays) {
    gl->GenVertexArrays (1, &resource->vao);
    gl->BindVertexArray (resource->vao);
  }

  if (!resource->vertex_buffer) {
    gl->GenBuffers (1, &resource->vertex_buffer);
    gl->BindBuffer (GL_ARRAY_BUFFER, resource->vertex_buffer);
    if (!flipped) {
      gl->BufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(GLfloat), vertices,
        GL_STATIC_DRAW);
    }
    else {
      gl->BufferData(GL_ARRAY_BUFFER, 4 * 5 * sizeof(GLfloat), flip_vertices,
        GL_STATIC_DRAW);
    }
  }

  if (!resource->vbo_indices) {
    gl->GenBuffers (1, &resource->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, resource->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
        GL_STATIC_DRAW);
  }

  if (gl->GenVertexArrays) {
    _bind_buffer (resource, gl);
    gl->BindVertexArray (0);
  }

  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);
}

static gboolean init_d3d_context(GstGLContext * gl_context, SharedResource* resource, HANDLE shtex_handle) {

  GstDXGID3D11Context * ctx = get_dxgi_share_context(gl_context);
  if (!ctx) {
    GST_ERROR("missing shared context");
    return FALSE;
  }
  resource->gl_device_handle = ctx->device_interop_handle;

  HRESULT hr = (ctx->d3d11_device)->lpVtbl->OpenSharedResource(
      ctx->d3d11_device,
      shtex_handle,
      &IID_ID3D11Texture2D,
      (void**)&resource->d3d_texture);

  return (hr == S_OK);
}

static void create_gl_texture(SharedResource* resource, GstGLContext* gl_context) {
  const GstGLFuncs* gl = gl_context->gl_vtable;
  GstDXGID3D11Context * ctx = get_dxgi_share_context(gl_context);

  gl->GenTextures(1, &resource->gl_texture);
  if (gl->GetError() != 0) GST_ERROR("Failed glBindTexture");

  resource->gl_texture_handle = ctx->wglDXRegisterObjectNV(resource->gl_device_handle,
      resource->d3d_texture, resource->gl_texture,
      GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);

  gl->BindTexture(GL_TEXTURE_2D, resource->gl_texture);
  if (gl->GetError() != 0) GST_ERROR("Failed glBindTexture");

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

static void
shared_resource_draw_frame(SharedResource* resource, GstGLContext* gl_context) {
  const GstGLFuncs* gl = gl_context->gl_vtable;
  GstDXGID3D11Context * ctx = get_dxgi_share_context(gl_context);

  // start new shits
  gl->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl->Clear(GL_COLOR_BUFFER_BIT);

  gl->Enable(GL_BLEND);
  gl->BlendFunc(GL_ONE, GL_ONE);

  gst_gl_shader_use(resource->display_shader);
  gst_gl_shader_set_uniform_1i (resource->display_shader, "tex", 0);

  if (gl->GenVertexArrays) {
      gl->BindVertexArray (resource->vao);
  }
  _bind_buffer (resource, gl);

  gl->ActiveTexture (GL_TEXTURE0);
  gl->BindTexture(GL_TEXTURE_2D, resource->gl_texture);

  ctx->wglDXLockObjectsNV(resource->gl_device_handle, 1, &resource->gl_texture_handle);
  gl->DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
  ctx->wglDXUnlockObjectsNV(resource->gl_device_handle, 1, &resource->gl_texture_handle);

  gl->BindTexture (GL_TEXTURE_2D, 0);

  gl->Disable(GL_BLEND);
  gl->BlendFunc(GL_ONE, GL_ZERO);

  gst_gl_context_clear_shader (gl_context);

  if (gl->GenVertexArrays) {
    gl->BindVertexArray (0);
  }

  _unbind_buffer (resource, gl);
}

gboolean init_shared_resource(GstGLContext* gl_context, HANDLE shtex_handle, 
    void** resource_out, gboolean flip) {
  SharedResource* resource = g_new0(SharedResource, 1);
  resource->draw_frame = shared_resource_draw_frame;

  init_display_shader(resource, gl_context, flip);

  GstDXGID3D11Context * ctx = get_dxgi_share_context(gl_context);

  if (!init_d3d_context(gl_context, resource, shtex_handle)) {
    free_shared_resource(gl_context, resource);
    return FALSE;
  }

  create_gl_texture(resource, gl_context);

  *resource_out = resource;
  return TRUE;
}

// must be called on gl thread
void free_shared_resource(GstGLContext* gl_context, SharedResource* resource) {
  const GstGLFuncs* gl = gl_context->gl_vtable;

  if (resource->gl_device_handle) {
    if (resource->gl_texture) {
      GstDXGID3D11Context * ctx = get_dxgi_share_context(gl_context);
      ctx->wglDXUnregisterObjectNV(resource->gl_device_handle, 
          resource->gl_texture_handle);
    }
  }

  if (resource->display_shader) {
    gst_object_unref(resource->display_shader);
  }

  if (resource->vertex_buffer) {
    gl->DeleteBuffers(1, &resource->vertex_buffer);
  }

  if (resource->vbo_indices) {
    gl->DeleteBuffers(1, &resource->vbo_indices);
  }

  if (resource->vao) {
    gl->DeleteVertexArrays(1, &resource->vao);
  }

  g_free(resource);
}
