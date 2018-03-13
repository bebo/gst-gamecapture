#ifndef CHOCOBO_GAME_CAPTURE_H_
#define CHOCOBO_GAME_CAPTURE_H_

#include <glib.h>
#include <tchar.h>
#include <windows.h>
#include <stdint.h>

#include <gst/gst.h>


#ifdef __cplusplus 
extern "C" {
#endif 

#if 0
GST_EXPORT GstDebugCategory *gst_chocobopushsrc_debug;
#define GST_CAT_DEFAULT gst_chocobopushsrc_debug
#endif

typedef struct _GameCaptureConfig GameCaptureConfig;
struct _GameCaptureConfig {
    char                          *title;
    char                          *klass;
    char                          *executable;
    char                          *binary_path;
    enum window_priority          priority;
    enum capture_mode             mode;
    uint32_t                      scale_cx;
    uint32_t                      scale_cy;
    gboolean                      cursor;
    gboolean                      force_shmem;
    gboolean                      force_scaling;
    gboolean                      allow_transparency;
    gboolean                      limit_framerate;
    gboolean                      capture_overlays;
    gboolean                      anticheat_hook;
    HWND                          window;
};

gboolean game_capture_is_ready(void * data);
gboolean game_capture_is_active(void * data);
void* game_capture_get_shtex_handle(void * data);
void* game_capture_start(void **data, 
    char* window_class_name, char* window_name,
    GameCaptureConfig *config, uint64_t frame_interval);
gboolean game_capture_tick(void * data);
gboolean game_capture_stop(void * data);
void set_fps(void **data, uint64_t frame_interval);

wchar_t *get_wc(const char *c);
#ifdef __cplusplus
}
#endif

#endif // CHOCOBO_GAME_CAPTURE_H_
