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
  ID3D11Resource *resource;
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
      __uuidof(ID3D11Texture2D), (void**)&resource);

#if 0
  ID3D11Texture2D* shared_texture = (ID3D11Texture2D*)1;
  wglDXSetResourceShareHandleNV(shared_texture, handle);
#endif

  GST_WARNING("JAKE OpenSharedResource HR: 0x%08x", hr);
}
