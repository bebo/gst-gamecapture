#include "d3d_chocobo.h"

#include <windows.h>
#include <GL/glew.h>
#include <GL/wglew.h>

const static D3D_FEATURE_LEVEL featureLevels[] =
{
  D3D_FEATURE_LEVEL_11_0,
  D3D_FEATURE_LEVEL_10_1,
  D3D_FEATURE_LEVEL_10_0,
  D3D_FEATURE_LEVEL_9_3,
};


void init_gl() {
#if 0
  HWND hWndGL = CreateWindowEx(0, TEXT("WindowClass"),
      0, 0, 0, 0,
      0, 0, 0, 0,
      GetModuleHandle(NULL), 
      0);

  PIXELFORMATDESCRIPTOR pfd = {};
  pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_SUPPORT_OPENGL;

  HDC g_hDCGL = GetDC(hWndGL);
  GLuint PixelFormat = ChoosePixelFormat(g_hDCGL, &pfd);
  SetPixelFormat(g_hDCGL, PixelFormat, &pfd);
  HGLRC hRC = wglCreateContext(g_hDCGL);
  wglMakeCurrent(g_hDCGL, hRC);

#endif

  PIXELFORMATDESCRIPTOR pfd;
  HWND hwnd; HDC hdc; int pixelFormat;
  memset(&pfd,0,sizeof(PIXELFORMATDESCRIPTOR));
  pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 16;
  pfd.iLayerType = PFD_MAIN_PLANE;

  hwnd = CreateWindowEx(0,
      "STATIC", NULL,
      0,
      0,0,0,0,0,0,
      GetModuleHandle(NULL),0); 

  hdc = GetDC(hwnd);
  pixelFormat = ChoosePixelFormat(hdc,&pfd);
  SetPixelFormat(hdc,pixelFormat,&pfd);
  wglMakeCurrent(hdc,wglCreateContext(hdc));

  GLenum x = glewInit();
  GST_INFO("glewInit: %d", (int) x);

  GST_INFO("VENDOR : %s", glGetString(GL_VENDOR));
  GST_INFO("RENDERER : %s", glGetString(GL_RENDERER));
  GST_INFO("VERSION : %s", glGetString(GL_VERSION));

  GST_INFO("wglGetCurrentContext is NULL: %d", wglGetCurrentContext() == NULL);
  GST_INFO("wglGetCurrentDC is NULL: %d", wglGetCurrentDC() == NULL);
}

void d3d11_create_device() {
  GST_INFO("d3d11 create device");

  init_gl();

  GST_INFO("wglDXOpenDeviceNV is NULL: %d", (wglDXOpenDeviceNV == NULL));

  ID3D11Device *device;
  ID3D11Texture2D *texture;
  ID3D11DeviceContext *context;
//  IDXGIAdapter *adapter;

  D3D_FEATURE_LEVEL level_used = D3D_FEATURE_LEVEL_9_3;

#if 0
  HANDLE handle = (HANDLE)1073748418;
#else
  HANDLE handle = (HANDLE)1073747010;
#endif

  // FIXME: D3D11CreateDevice, not pass null as adapter
#if 1
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
#else
  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      NULL,                        // pAdapter
      D3D_DRIVER_TYPE_HARDWARE,    // DriverType
      NULL,                        // Software
      0, // Flags (Do not set D3D11_CREATE_DEVICE_SINGLETHREADED)
      NULL,                                    // pFeatureLevels
      0, // FeatureLevels
      D3D11_SDK_VERSION,           // SDKVersion
      &sd,                        // pSwapChainDesc
      &swapchain,                  // ppSwapChain
      &device,                     // ppDevice
      &level_used,                 // pFeatureLevel
      &context);                   // ppImmediateContext
#endif

  GST_WARNING("JAKE CreateDevice HR: 0x%08x, level_used: %d", hr, 
      (unsigned int) level_used);

  hr = device->OpenSharedResource(handle,
      __uuidof(ID3D11Texture2D), (void**)&texture);

  GST_WARNING("JAKE OpenSharedResource HR: 0x%08x", hr);

#if 1
  HANDLE gl_handleD3D = wglDXOpenDeviceNV(device);

  GST_WARNING("JAKE wglDXOpenDeviceNV %llu", gl_handleD3D);
  HANDLE gl_handles[2];
  GLuint gl_names[2];

  BOOL success = wglDXSetResourceShareHandleNV(texture, handle);

  glGenTextures(1, gl_names);
  if (glGetError() != 0) GST_ERROR("Failed glGenTextures");

  gl_handles[0] = wglDXRegisterObjectNV(gl_handleD3D, texture,
      gl_names[0],
      GL_TEXTURE_2D,
      WGL_ACCESS_READ_WRITE_NV);

  glBindTexture(GL_TEXTURE_2D, gl_names[0]);
  if (glGetError() != 0) GST_ERROR("Failed glBindTexture");

  //no filtering
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  if (glGetError() != 0) GST_ERROR("Failed glTexParameteri1");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  if (glGetError() != 0) GST_ERROR("Failed glTexParameteri2");

  glGenerateMipmap(GL_TEXTURE_2D);
  if (glGetError() != 0) GST_ERROR("Failed glGenerateMipmap");

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  if (glGetError() != 0) GST_ERROR("Failed glGenFramebuffers");

  // attach the Direct3D buffers to an FBO
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  if (glGetError() != 0) GST_ERROR("Failed glBindFramebuffer");

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
      GL_TEXTURE_2D, gl_names[0], 0);
  if (glGetError() != 0) GST_ERROR("Failed glFramebufferTexture2D");

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
#endif

}
