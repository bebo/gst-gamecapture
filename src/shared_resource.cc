#include "shared_resource.h"

/* *INDENT-OFF* */
static const GLfloat vertices[] = {
  -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
  1.0f,  1.0f,  0.0f, 1.0f, 1.0f,
  -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
  1.0f, -1.0f,  0.0f, 1.0f, 0.0f
};

static const GLushort indices[] = { 0, 1, 2, 1, 3, 2 };
/* *INDENT-ON* */

const static D3D_FEATURE_LEVEL d3d_feature_levels[] =
{
  D3D_FEATURE_LEVEL_11_0,
  D3D_FEATURE_LEVEL_10_1,
  D3D_FEATURE_LEVEL_10_0,
  D3D_FEATURE_LEVEL_9_3,
};

static PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV;
static PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV;
static PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV;
static PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV;
static PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV;
static PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV;
static PFNWGLDXSETRESOURCESHAREHANDLENVPROC wglDXSetResourceShareHandleNV;

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

static void init_wgl_functions(GstGLContext* gl_context) {
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
}

static void init_display_shader(SharedResource* resource, GstGLContext* gl_context) {
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
    gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), vertices,
        GL_STATIC_DRAW);
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


static ID3D11Device* create_device_d3d11() {
  ID3D11Device* device;

  IDXGIFactory1* factory;
  HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**) &factory);

  UINT index = 0;
  IDXGIAdapter1* adapter;
  while (factory->EnumAdapters1(index++, &adapter) == S_OK) {
    DXGI_ADAPTER_DESC desc;
    hr = adapter->GetDesc(&desc);

    if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c) {
      adapter->Release();
      continue;
    }
    break;
  }
  factory->Release();

  D3D_FEATURE_LEVEL level_used = D3D_FEATURE_LEVEL_9_3;

  hr = D3D11CreateDevice(adapter, 
      D3D_DRIVER_TYPE_UNKNOWN,
      NULL, 
      D3D11_CREATE_DEVICE_BGRA_SUPPORT, 
      d3d_feature_levels,
      sizeof(d3d_feature_levels) / sizeof(D3D_FEATURE_LEVEL),
      D3D11_SDK_VERSION,
      &device,
      &level_used, 
      NULL);
  adapter->Release();

  //GST_INFO("CreateDevice HR: 0x%08x, level_used: 0x%08x (%d)", hr,
  //    (unsigned int) level_used, (unsigned int) level_used);
  return device;
}

static gboolean init_devices(SharedResource* resource) {
  resource->d3d_device = create_device_d3d11();
  if (resource->d3d_device == NULL) {
    GST_ERROR("Failed to create d3d11 device");
    return FALSE;
  }

  resource->gl_device_handle = wglDXOpenDeviceNV(resource->d3d_device);
  return TRUE;
}

static void create_gl_texture(SharedResource* resource, GstGLContext* gl_context) {
  const GstGLFuncs* gl = gl_context->gl_vtable;

  wglDXSetResourceShareHandleNV(resource->d3d_texture,
      resource->d3d_shared_handle);

  gl->GenTextures(1, &resource->gl_texture);

  resource->gl_texture_handle = wglDXRegisterObjectNV(resource->gl_device_handle,
      resource->d3d_texture, resource->gl_texture,
      GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);
  // GST_INFO("wglDXRegisterObjectNV texture handle: %llu texture: %llu", 
  //   resource->gl_texture_handle, resource->gl_texture);

  gl->BindTexture(GL_TEXTURE_2D, resource->gl_texture);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->BindTexture(GL_TEXTURE_2D, 0);
}

static gboolean open_shared_texture(SharedResource* resource, GstGLContext* gl_context) {
  ID3D11Device* device = (ID3D11Device*) resource->d3d_device;
  HRESULT hr = device->OpenSharedResource(resource->d3d_shared_handle,
      __uuidof(ID3D11Texture2D), (void**)&resource->d3d_texture);

  if (hr != S_OK) {
    return FALSE;
  }

  create_gl_texture(resource, gl_context);
  return TRUE;
}


static void
shared_resource_draw_frame(SharedResource* resource, GstGLContext* gl_context) {
  const GstGLFuncs* gl = gl_context->gl_vtable;

  // handle the case where theres no gl texture attached to it yet
  if (resource->gl_texture == 0) {
    if (!open_shared_texture(resource, gl_context)) {
      gl->ClearColor(0.92f, 0.22f, 0.25f, 1.0f);
      gl->Clear(GL_COLOR_BUFFER_BIT);
      return;
    }
  }

  // start new shits
  gl->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl->Clear(GL_COLOR_BUFFER_BIT);

  gl->Enable(GL_BLEND);
  gl->BlendFunc(GL_ONE, GL_ONE);

  gst_gl_shader_use(resource->display_shader);
  gst_gl_shader_set_uniform_1i(resource->display_shader, "tex", 0);

  if (gl->GenVertexArrays) {
    gl->BindVertexArray(resource->vao);
  }
  _bind_buffer(resource, gl);

  gl->ActiveTexture(GL_TEXTURE0);
  gl->BindTexture(GL_TEXTURE_2D, resource->gl_texture);

  wglDXLockObjectsNV(resource->gl_device_handle, 1, &resource->gl_texture_handle);
  gl->DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
  wglDXUnlockObjectsNV(resource->gl_device_handle, 1, &resource->gl_texture_handle);

  gl->BindTexture(GL_TEXTURE_2D, 0);

  gl->Disable(GL_BLEND);
  gl->BlendFunc(GL_ONE, GL_ZERO);

  gst_gl_context_clear_shader(gl_context);

  if (gl->GenVertexArrays) {
    gl->BindVertexArray(0);
  }

  _unbind_buffer (resource, gl);
}

gboolean init_shared_resource(GstGLContext* gl_context, HANDLE shtex_handle, 
    void** resource_out) {
  SharedResource* resource = g_new0(SharedResource, 1);
  resource->draw_frame = shared_resource_draw_frame;
  resource->d3d_shared_handle = shtex_handle;

  init_wgl_functions(gl_context);
  init_display_shader(resource, gl_context);
  init_devices(resource);

  open_shared_texture(resource, gl_context);

  *resource_out = resource;
  return TRUE;
}

// must be called on gl thread
void free_shared_resource(GstGLContext* gl_context, SharedResource* resource) {
  const GstGLFuncs* gl = gl_context->gl_vtable;

  if (resource->gl_device_handle) {
    if (resource->gl_texture) {
      wglDXUnregisterObjectNV(resource->gl_device_handle, 
          resource->gl_texture_handle);
    }

    wglDXCloseDeviceNV(resource->gl_device_handle);
  }

  if (resource->d3d_device) {
    ((ID3D11Device*) resource->d3d_device)->Release();
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
