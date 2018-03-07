#include "d3d_chocobo.h"

#include <GL/glew.h>
#include <GL/wglew.h>

const static D3D_FEATURE_LEVEL featureLevels[] =
{
  D3D_FEATURE_LEVEL_11_0,
  D3D_FEATURE_LEVEL_10_1,
  D3D_FEATURE_LEVEL_10_0,
  D3D_FEATURE_LEVEL_9_3,
};


void d3d11_create_device() {
  GST_INFO("d3d11 create device");

  ID3D11Device *device;
  ID3D11Texture2D *texture;
  ID3D11DeviceContext *context;
//  IDXGIAdapter *adapter;

  D3D_FEATURE_LEVEL level_used = D3D_FEATURE_LEVEL_9_3;

  HANDLE handle = (HANDLE)1073750082;

  // FIXME: D3D11CreateDevice, not pass null as adapter
  HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE,
      NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels,
      sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL),
      D3D11_SDK_VERSION, &device,
      &level_used, &context);

  GST_WARNING("JAKE CreateDevice HR: 0x%08x, level_used: %d", hr, 
      (unsigned int) level_used);

  hr = device->OpenSharedResource(handle,
      __uuidof(ID3D11Texture2D), (void**)&texture);

#if 0
  HANDLE gl_handleD3D = wglDXOpenDeviceNV(device);
  HANDLE gl_handles[2];
  GLuint gl_names[2];

  BOOL success = wglDXSetResourceShareHandleNV(texture, handle);

  glGenTextures(1, gl_names);

  gl_handles[0] = wglDXRegisterObjectNV(gl_handleD3D, texture,
      gl_names[0],
      GL_TEXTURE_2D,
      WGL_ACCESS_READ_WRITE_NV);

  glBindTexture(GL_TEXTURE_2D, gl_names[0]);

  //no filtering
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glGenerateMipmap(GL_TEXTURE_2D);

  GLuint fbo;
  glGenFramebuffers(1, &fbo);

  // attach the Direct3D buffers to an FBO
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
      GL_TEXTURE_2D, gl_names[0], 0);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
#endif

  GST_WARNING("JAKE OpenSharedResource HR: 0x%08x", hr);
}
