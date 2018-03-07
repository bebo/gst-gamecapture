#ifndef D3D_CHOCOBO_H_
#define D3D_CHOCOBO_H_

#include <d3d11.h>
#include <dxgi.h>
#include <gst/gst.h>

#ifdef __cplusplus 
extern "C" {
#endif 

GST_EXPORT GstDebugCategory *gst_chocobopushsrc_debug;
#define GST_CAT_DEFAULT gst_chocobopushsrc_debug
  

void d3d11_create_device();

#ifdef __cplusplus
}
#endif

#endif /* D3D_CHOCOBO_H_ */
