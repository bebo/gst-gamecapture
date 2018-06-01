#ifndef CHOCOBO_GAME_CAPTURE_H_
#define CHOCOBO_GAME_CAPTURE_H_

#include <glib.h>
#include <tchar.h>
#include <windows.h>
#include <stdint.h>
#include <ipc-util/pipe.h>
#include "gamecapture/window-helpers.h"
#include <gst/gst.h>


#ifdef __cplusplus 
extern "C" {
#endif 

#if 1
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

struct game_capture {
  //struct cursor_data            cursor_data;
  HANDLE                        injector_process;
  uint32_t                      cx;
  uint32_t                      cy;
  uint32_t                      pitch;
  DWORD                         process_id;
  DWORD                         thread_id;
  HWND                          next_window;
  HWND                          window;
  uint64_t                      next_retry_time_ns;
  struct dstr                   title;
  struct dstr                   klass;
  struct dstr                   executable;
  enum window_priority          priority;
  LONG64                        frame_interval;
  bool                          wait_for_target_startup;
  bool                          showing;
  bool                          active;
  bool                          capturing;
  bool                          activate_hook;
  bool                          process_is_64bit;
  bool                          error_acquiring;
  bool                          dwm_capture;
  bool                          initial_config;
  bool                          convert_16bit;
  bool                          is_app;

  GameCaptureConfig           config;

  ipc_pipe_server_t             pipe;
  struct hook_info              *global_hook_info;
  HANDLE                        keepalive_mutex;
  HANDLE                        hook_init;
  HANDLE                        hook_restart;
  HANDLE                        hook_stop;
  HANDLE                        hook_ready;
  HANDLE                        hook_exit;
  HANDLE                        hook_data_map;
  HANDLE                        global_hook_info_map;
  HANDLE                        target_process;
  HANDLE                        texture_mutexes[2];
  wchar_t                       *app_sid;
  int                           retrying;

  union {
    struct {
      struct shmem_data *shmem_data;
      uint8_t *texture_buffers[2];
    };

    struct shtex_data *shtex_data;
    void *data;
  };
};

gboolean game_capture_shmem_draw_frame(struct game_capture* ga, uint8_t* data, uint32_t stride);
gboolean game_capture_is_ready(void * data);
gboolean game_capture_is_active(void * data);
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
