#include "d3d_chocobo.h"

#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720

const static D3D_FEATURE_LEVEL featureLevels[] =
{
  D3D_FEATURE_LEVEL_11_0,
  D3D_FEATURE_LEVEL_10_1,
  D3D_FEATURE_LEVEL_10_0,
  D3D_FEATURE_LEVEL_9_3,
};

#if 0
  HANDLE handle = (HANDLE)3221245890;
#else
  HANDLE handle = (HANDLE)2147508610;
#endif

void* d3d11_create_device() {
  GST_INFO("d3d11 create device");

  Chocobo* instance = g_new(Chocobo, 1);

  // initialize gl context
  instance->gl_context = gl_new_context();

  // get d3d11 device
  ID3D11Device *device;
  ID3D11Texture2D *texture;
  ID3D11DeviceContext *context;
//  IDXGIAdapter *adapter;

  D3D_FEATURE_LEVEL level_used = D3D_FEATURE_LEVEL_9_3;

  // FIXME: D3D11CreateDevice, not pass null as adapter
  HRESULT hr = D3D11CreateDevice(NULL, 
      D3D_DRIVER_TYPE_HARDWARE,
      NULL, 
      D3D11_CREATE_DEVICE_BGRA_SUPPORT, 
      featureLevels,
      sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL),
      D3D11_SDK_VERSION, 
      &device,
      &level_used, 
      &context);
  GST_INFO("CreateDevice HR: 0x%08x, level_used: %d", hr, 
      (unsigned int) level_used);

  hr = device->OpenSharedResource(handle,
      __uuidof(ID3D11Texture2D), (void**)&texture);
  GST_INFO("OpenSharedResource HR: 0x%08x", hr);

  instance->gl_context->native_texture = texture;
  instance->gl_context->native_device = device;
  instance->gl_context->native_shared_surface_handle = handle;

  return instance;
}
