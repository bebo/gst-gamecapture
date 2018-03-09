#ifndef D3D_CHOCOBO_H_
#define D3D_CHOCOBO_H_

#include <gst/gst.h>
#include "gl_context.h"

#ifdef __cplusplus 
extern "C" {
#endif 

GST_EXPORT GstDebugCategory *gst_chocobopushsrc_debug;
#define GST_CAT_DEFAULT gst_chocobopushsrc_debug

typedef struct _Chocobo Chocobo;
struct _Chocobo {
  GLContext* gl_context;
};

void* d3d11_create_device();

#ifdef __cplusplus
}
#endif

#endif /* D3D_CHOCOBO_H_ */
