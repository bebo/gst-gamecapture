#include "game_capture.h"

#include <chrono>
#include <dshow.h>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>
#include <string>
#include <dxgi.h>
#include <ipc-util/pipe.h>
#include <libyuv/convert_from_argb.h>
#include "gamecapture/graphics-hook-info.h"
#include "gamecapture/bmem.h"
#include "gamecapture/dstr.h"
#include "gamecapture/app-helpers.h"
#include "gamecapture/platform.h"
#include "gamecapture/threading.h"
#include "gamecapture/obfuscate.h"
#include "gamecapture/nt-stuff.h"
#include "gamecapture/inject-library.h"
#include "gamecapture/window-helpers.h"

#define STOP_BEING_BAD \
  "This is most likely due to security software" \
  "that the Bebo Capture installation folder is excluded/ignored in the " \
  "settings of the security software you are using."

#if 1
#define debug(...) GST_INFO(__VA_ARGS__)
#define info(...) GST_INFO(__VA_ARGS__)
#define warn(...) GST_WARNING(__VA_ARGS__)
#define error(...) GST_ERROR(__VA_ARGS__)
#else
#define debug(...)
#define info(...)
#define warn(...)
#define error(...)
#endif

extern "C" {
  extern char* dll_inject_path;
  struct graphics_offsets offsets32 = { 0 };
  struct graphics_offsets offsets64 = { 0 };

  char *bebo_find_file(const char *file_c) {
    std::string dll_path(dll_inject_path ?
        dll_inject_path : "C:\\Program Files (x86)\\Bebo\\bebodlls");
    const char* dll_path_c = dll_path.c_str();
    char* result = (char*) bmalloc(strlen(dll_path_c) + strlen(file_c) + 1);
    wsprintfA(result, "%s\\%s", dll_path_c, file_c);
    return result;
  }
}

enum capture_mode {
  CAPTURE_MODE_ANY,
  CAPTURE_MODE_WINDOW,
  CAPTURE_MODE_HOTKEY
};

static uint32_t inject_failed_count = 0;

static inline int inject_library(HANDLE process, const wchar_t *dll)
{
  return inject_library_obf(process, dll,
      "D|hkqkW`kl{k\\osofj", 0xa178ef3655e5ade7,
      "[uawaRzbhh{tIdkj~~", 0x561478dbd824387c,
      "[fr}pboIe`dlN}", 0x395bfbc9833590fd,
      "\\`zs}gmOzhhBq", 0x12897dd89168789a,
      "GbfkDaezbp~X", 0x76aff7238788f7db);
}

static inline bool use_anticheat(struct game_capture *gc)
{
  return gc->config.anticheat_hook && !gc->is_app;
}

static inline HANDLE open_mutex_plus_id(struct game_capture *gc,
    const wchar_t *name, DWORD id)
{
  wchar_t new_name[64];
  _snwprintf(new_name, 64, L"%s%lu", name, id);
  return gc->is_app
    ? open_app_mutex(gc->app_sid, new_name)
    : open_mutex(new_name);
}

static inline HANDLE open_mutex_gc(struct game_capture *gc,
    const wchar_t *name)
{
  return open_mutex_plus_id(gc, name, gc->process_id);
}

static inline HANDLE open_event_plus_id(struct game_capture *gc,
    const wchar_t *name, DWORD id)
{
  wchar_t new_name[64];
  _snwprintf(new_name, 64, L"%s%lu", name, id);
  return gc->is_app
    ? open_app_event(gc->app_sid, new_name)
    : open_event(new_name);
}

static inline HANDLE open_event_gc(struct game_capture *gc,
    const wchar_t *name)
{
  return open_event_plus_id(gc, name, gc->process_id);
}

static inline HANDLE open_map_plus_id(struct game_capture *gc,
    const wchar_t *name, DWORD id)
{
  wchar_t new_name[64];
  _snwprintf(new_name, 64, L"%s%lu", name, id);

  return gc->is_app
    ? open_app_map(gc->app_sid, new_name)
    : OpenFileMappingW(GC_MAPPING_FLAGS, false, new_name);
}

static inline HANDLE open_hook_info(struct game_capture *gc)
{
  return open_map_plus_id(gc, SHMEM_HOOK_INFO, gc->process_id);
}

static inline uint64_t get_next_retry_time(uint64_t milliseconds) {
  return os_gettime_ns() + (milliseconds * 1000000);
}

static struct game_capture *game_capture_create(GameCaptureConfig *config, uint64_t frame_interval)
{
  struct game_capture *gc = (struct game_capture*) g_new0(game_capture, 1);

  gc->config.priority = config->priority;
  gc->config.mode = config->mode;
  gc->config.scale_cx = config->scale_cx;
  gc->config.scale_cy = config->scale_cy;
  gc->config.cursor = config->cursor;
  gc->config.force_shmem = config->force_shmem;
  gc->config.force_scaling = config->force_scaling;
  gc->config.allow_transparency = config->allow_transparency;
  gc->config.limit_framerate = config->limit_framerate;
  gc->config.capture_overlays = config->capture_overlays;
  gc->config.anticheat_hook = inject_failed_count > 10 ? true : config->anticheat_hook;
  gc->frame_interval = frame_interval;

  gc->initial_config = true;
  gc->priority = config->priority;
  gc->wait_for_target_startup = false;
  gc->next_retry_time_ns = 0;
  gc->window = config->window;

  return gc;
}


static void close_handle(HANDLE *p_handle)
{
  HANDLE handle = *p_handle;
  if (handle) {
    if (handle != INVALID_HANDLE_VALUE)
      CloseHandle(handle);
    *p_handle = NULL;
  }
}

static inline HMODULE kernel32(void)
{
  static HMODULE kernel32_handle = NULL;
  if (!kernel32_handle)
    kernel32_handle = GetModuleHandleW(L"kernel32");
  return kernel32_handle;
}

static inline HANDLE open_process(DWORD desired_access, bool inherit_handle,
    DWORD process_id)
{
  static HANDLE(WINAPI *open_process_proc)(DWORD, BOOL, DWORD) = NULL;
  if (!open_process_proc)
    open_process_proc = (HANDLE(__stdcall *) (DWORD, BOOL, DWORD)) get_obfuscated_func(kernel32(),
        "NuagUykjcxr", 0x1B694B59451ULL);

  return open_process_proc(desired_access, inherit_handle, process_id);
}

static void setup_window(struct game_capture *gc, HWND window)
{
  HANDLE hook_restart;
  HANDLE process;

  GetWindowThreadProcessId(window, &gc->process_id);
  if (gc->process_id) {
    process = open_process(PROCESS_QUERY_INFORMATION,
        false, gc->process_id);
    if (process) {
      gc->is_app = is_app(process);
      if (gc->is_app) {
        gc->app_sid = get_app_sid(process);
      }
      CloseHandle(process);
    }
  }

  /* do not wait if we're re-hooking a process */
  hook_restart = open_event_gc(gc, EVENT_CAPTURE_RESTART);
  if (hook_restart) {
    gc->wait_for_target_startup = false;
    CloseHandle(hook_restart);
  }

  /* otherwise if it's an unhooked process, always wait a bit for the
   * target process to start up before starting the hook process;
   * sometimes they have important modules to load first or other hooks
   * (such as steam) need a little bit of time to load.  ultimately this
   * helps prevent crashes */
  if (gc->wait_for_target_startup) {
    gc->next_retry_time_ns = get_next_retry_time(3000); // 3 seconds
    gc->wait_for_target_startup = false;
  } else {
    gc->next_window = window;
  }
}

static void get_fullscreen_window(struct game_capture *gc)
{
  HWND window = GetForegroundWindow();
  MONITORINFO mi = { 0 };
  HMONITOR monitor;
  DWORD styles;
  RECT rect;

  gc->next_window = NULL;

  if (!window) {
    return;
  }
  if (!GetWindowRect(window, &rect)) {
    return;
  }

  /* ignore regular maximized windows */
  styles = (DWORD)GetWindowLongPtr(window, GWL_STYLE);
  if ((styles & WS_MAXIMIZE) != 0 && (styles & WS_BORDER) != 0) {
    return;
  }

  monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
  if (!monitor) {
    return;
  }

  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfo(monitor, &mi)) {
    return;
  }

  if (rect.left == mi.rcMonitor.left   &&
      rect.right == mi.rcMonitor.right  &&
      rect.bottom == mi.rcMonitor.bottom &&
      rect.top == mi.rcMonitor.top) {
    setup_window(gc, window);
  }
  else {
    gc->wait_for_target_startup = true;
  }
}

static void get_selected_window(struct game_capture *gc)
{
  HWND window;

  if (dstr_cmpi(&gc->klass, "dwm") == 0) {
    wchar_t class_w[512];
    os_utf8_to_wcs(gc->klass.array, 0, class_w, 512);
    window = FindWindowW(class_w, NULL);
  }
  else {
    window = find_window(INCLUDE_MINIMIZED,
        gc->priority,
        gc->klass.array,
        gc->title.array,
        gc->executable.array);
  }

  if (window) {
    setup_window(gc, window);
  }
  else {
    gc->wait_for_target_startup = true;
  }
}

static inline bool hook_direct(struct game_capture *gc,
    const char *hook_path_rel)
{
  wchar_t hook_path_abs_w[MAX_PATH];
  wchar_t *hook_path_rel_w;
  wchar_t *path_ret;
  HANDLE process;
  int ret;

  os_utf8_to_wcs_ptr(hook_path_rel, 0, &hook_path_rel_w);
  if (!hook_path_rel_w) {
    warn("hook_direct: could not convert string");
    return false;
  }

  path_ret = _wfullpath(hook_path_abs_w, hook_path_rel_w, MAX_PATH);
  bfree(hook_path_rel_w);

  if (path_ret == NULL) {
    warn("hook_direct: could not make absolute path");
    return false;
  }

  process = open_process(PROCESS_ALL_ACCESS, false, gc->process_id);
  if (!process) {
    warn("hook_direct: could not open process: %s (%lu) %s, %s",
        gc->config.executable, GetLastError(), gc->config.title, gc->config.klass);
    return false;
  }

  ret = inject_library(process, hook_path_abs_w);
  CloseHandle(process);

  if (ret != 0) {
    error("hook_direct: inject failed: %d, anti_cheat: %d, %s, %s, %s", ret, gc->config.anticheat_hook, gc->config.title, gc->config.klass, gc->config.executable);
    if (ret == INJECT_ERROR_UNLIKELY_FAIL) {
      inject_failed_count++;
    }
    return false;
  }

  return true;
}

static const char *blacklisted_exes[] = {
  "explorer",
  "steam",
  "battle.net",
  "galaxyclient",
  "skype",
  "uplay",
  "origin",
  "devenv",
  "taskmgr",
  "chrome",
  "firefox",
  "systemsettings",
  "applicationframehost",
  "cmd",
  "bebo",
  "epicgameslauncher",
  "shellexperiencehost",
  "winstore.app",
  "searchui",
  NULL
};
static bool is_blacklisted_exe(const char *exe)
{
  char cur_exe[MAX_PATH];

  if (!exe)
    return false;

  for (const char **vals = blacklisted_exes; *vals; vals++) {
    strcpy(cur_exe, *vals);
    strcat(cur_exe, ".exe");

    if (_strcmpi(cur_exe, exe) == 0)
      return true;
  }

  return false;
}

static bool target_suspended(struct game_capture *gc)
{
  return thread_is_suspended(gc->process_id, gc->thread_id);
}

static inline bool is_64bit_windows(void)
{
#ifdef _WIN64
  return true;
#else
  BOOL x86 = false;
  bool success = !!IsWow64Process(GetCurrentProcess(), &x86);
  return success && !!x86;
#endif
}

static inline bool is_64bit_process(HANDLE process)
{
  BOOL x86 = true;
  if (is_64bit_windows()) {
    bool success = !!IsWow64Process(process, &x86);
    if (!success) {
      return false;
    }
  }

  return !x86;
}

static inline bool open_target_process(struct game_capture *gc)
{
  gc->target_process = open_process(
      PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
      false, gc->process_id);
  if (!gc->target_process) {
    warn("could not open process: %S (%lu) %S, %S",
        gc->config.executable, GetLastError(), gc->config.title, gc->config.klass);
    return false;
  }

  gc->process_is_64bit = is_64bit_process(gc->target_process);
  gc->is_app = is_app(gc->target_process);
  if (gc->is_app) {
    gc->app_sid = get_app_sid(gc->target_process);
  }
  return true;
}

static bool check_file_integrity(struct game_capture *gc, const char *file,
    const char *name)
{
  DWORD error;
  HANDLE handle;
  wchar_t *w_file = NULL;

  if (!file || !*file) {
    warn("Game capture %s not found.", STOP_BEING_BAD, name);
    return false;
  }

  if (!os_utf8_to_wcs_ptr(file, 0, &w_file)) {
    warn("Could not convert file name to wide string");
    return false;
  }

  handle = CreateFileW(w_file, GENERIC_READ | GENERIC_EXECUTE,
      FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

  bfree(w_file);

  if (handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle);
    return true;
  }

  error = GetLastError();
  if (error == ERROR_FILE_NOT_FOUND) {
    warn("Game capture file '%s' not found."
        STOP_BEING_BAD, file);
  }
  else if (error == ERROR_ACCESS_DENIED) {
    warn("Game capture file '%s' could not be loaded."
        STOP_BEING_BAD, file);
  }
  else {
    warn("Game capture file '%s' could not be loaded: %lu."
        STOP_BEING_BAD, file, error);
  }

  return false;
}

static inline bool create_inject_process(struct game_capture *gc,
    const char *inject_path, const char *hook_dll)
{
  wchar_t *command_line_w = (wchar_t *)malloc(4096 * sizeof(wchar_t));
  wchar_t *inject_path_w;
  wchar_t *hook_dll_w;
  bool anti_cheat = use_anticheat(gc);
  PROCESS_INFORMATION pi = { 0 };
  // STARTUPINFO si = { 0 };
  STARTUPINFOW si = { 0 };
  bool success = false;

  os_utf8_to_wcs_ptr(inject_path, 0, &inject_path_w);
  os_utf8_to_wcs_ptr(hook_dll, 0, &hook_dll_w);

  si.cb = sizeof(si);

  swprintf(command_line_w, 4096, L"\"%s\" \"%s\" %lu %lu",
      inject_path_w, hook_dll_w,
      (unsigned long)anti_cheat,
      anti_cheat ? gc->thread_id : gc->process_id);

  success = !!CreateProcessW(inject_path_w, command_line_w, NULL, NULL,
      false, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
  if (success) {
    CloseHandle(pi.hThread);
    gc->injector_process = pi.hProcess;
  }
  else {
    warn("Failed to create inject helper process: %s (%lu)",
        gc->config.executable, GetLastError());
  }

  free(command_line_w);
  bfree(inject_path_w);
  bfree(hook_dll_w);
  return success;
}

static inline bool inject_hook(struct game_capture *gc)
{
  bool matching_architecture;
  bool success = false;
  const char *hook_dll;
  char *inject_path;
  char *hook_path;

  if (gc->process_is_64bit) {
    hook_dll = "graphics-hook64.dll";
    inject_path = bebo_find_file("inject-helper64.exe");
  }
  else {
    hook_dll = "graphics-hook32.dll";
    inject_path = bebo_find_file("inject-helper32.exe");
  }

  hook_path = bebo_find_file(hook_dll);

  info("injecting %s with %s into %s", hook_dll, inject_path, gc->config.executable);

  if (!check_file_integrity(gc, inject_path, "inject helper")) {
    goto cleanup;
  }
  if (!check_file_integrity(gc, hook_path, "graphics hook")) {
    goto cleanup;
  }

#ifdef _WIN64
  matching_architecture = gc->process_is_64bit;
#else
  matching_architecture = !gc->process_is_64bit;
#endif

  if (matching_architecture && !use_anticheat(gc)) {
    info("using direct hook");
    success = hook_direct(gc, hook_path);

    if (!success && inject_failed_count > 10) {
      gc->config.anticheat_hook = true;
      info("hook_direct: inject failed for 10th time, retrying with helper (%S hook)", use_anticheat(gc) ?
          "compatibility" : "direct");
      success = create_inject_process(gc, inject_path, hook_dll);
    }
  }
  else {
    info("using helper (%s hook)", use_anticheat(gc) ? "compatibility" : "direct");
    success = create_inject_process(gc, inject_path, hook_dll);
  }

  if (success) {
    inject_failed_count = 0;
  }

cleanup:
  bfree(inject_path);
  bfree(hook_path);
  return success;
}

static inline bool init_keepalive(struct game_capture *gc)
{
  wchar_t new_name[64];
  _snwprintf(new_name, 64, L"%ls%lu", WINDOW_HOOK_KEEPALIVE,
      gc->process_id);

  gc->keepalive_mutex = CreateMutexW(NULL, false, new_name);
  if (!gc->keepalive_mutex) {
    warn("Failed to create keepalive mutex: %lu", GetLastError());
    return false;
  }
  return true;
}

static inline bool init_texture_mutexes(struct game_capture *gc)
{
  gc->texture_mutexes[0] = open_mutex_gc(gc, MUTEX_TEXTURE1);
  gc->texture_mutexes[1] = open_mutex_gc(gc, MUTEX_TEXTURE2);

  if (!gc->texture_mutexes[0] || !gc->texture_mutexes[1]) {
    DWORD error = GetLastError();
    if (error == 2) {
      if (!gc->retrying) {
        gc->retrying = 2;
        info("hook not loaded yet, retrying..");
      }
    } else {
      warn("failed to open texture mutexes: %lu",
          GetLastError());
    }
    return false;
  }

  return true;
}

static void pipe_log(void *param, uint8_t *data, size_t size)
{
  if (data && size)
    info("%s", data);
}

static inline bool init_pipe(struct game_capture *gc)
{
  char name[64];

  sprintf(name, "%s%lu", PIPE_NAME, gc->process_id);

  if (!ipc_pipe_server_start(&gc->pipe, name, pipe_log, gc)) {
    warn("init_pipe: failed to start pipe");
    return false;
  }

  return true;
}

static inline void reset_frame_interval(struct game_capture *gc)
{
  gc->global_hook_info->frame_interval = gc->frame_interval;
}

static inline bool init_hook_info(struct game_capture *gc)
{
  gc->global_hook_info_map = open_hook_info(gc);
  if (!gc->global_hook_info_map) {
    warn("init_hook_info: get_hook_info failed: %lu",
        GetLastError());
    return false;
  }

  gc->global_hook_info = (hook_info *)MapViewOfFile(gc->global_hook_info_map,
      FILE_MAP_ALL_ACCESS, 0, 0,
      sizeof(*gc->global_hook_info));
  if (!gc->global_hook_info) {
    warn("init_hook_info: failed to map data view: %lu",
        GetLastError());
    return false;
  }

  gc->global_hook_info->offsets = gc->process_is_64bit ? offsets64 : offsets32;
  gc->global_hook_info->capture_overlay = gc->config.capture_overlays;
  gc->global_hook_info->force_shmem = false;
  gc->global_hook_info->use_scale = gc->config.force_scaling;
  gc->global_hook_info->cx = gc->config.scale_cx;
  gc->global_hook_info->cy = gc->config.scale_cy;
  reset_frame_interval(gc);

  return true;
}

static inline bool init_events(struct game_capture *gc)
{
  if (!gc->hook_restart) {
    gc->hook_restart = open_event_gc(gc, EVENT_CAPTURE_RESTART);
    if (!gc->hook_restart) {
      warn("init_events: failed to get hook_restart "
          "event: %lu", GetLastError());
      return false;
    }
  }

  if (!gc->hook_stop) {
    gc->hook_stop = open_event_gc(gc, EVENT_CAPTURE_STOP);
    if (!gc->hook_stop) {
      warn("init_events: failed to get hook_stop event: %lu",
          GetLastError());
      return false;
    }
  }

  if (!gc->hook_init) {
    gc->hook_init = open_event_gc(gc, EVENT_HOOK_INIT);
    if (!gc->hook_init) {
      warn("init_events: failed to get hook_init event: %lu",
          GetLastError());
      return false;
    }
  }

  if (!gc->hook_ready) {
    gc->hook_ready = open_event_gc(gc, EVENT_HOOK_READY);
    if (!gc->hook_ready) {
      warn("init_events: failed to get hook_ready event: %lu",
          GetLastError());
      return false;
    }
  }

  if (!gc->hook_exit) {
    gc->hook_exit = open_event_gc(gc, EVENT_HOOK_EXIT);
    if (!gc->hook_exit) {
      warn("init_events: failed to get hook_exit event: %lu",
          GetLastError());
      return false;
    }
  }

  return true;
}

/* if there's already a hook in the process, then signal and start */
static inline bool attempt_existing_hook(struct game_capture *gc)
{
  gc->hook_stop = open_event_gc(gc, EVENT_CAPTURE_STOP);
  if (gc->hook_stop) {
    info("existing hook found, signaling process: %s",
        gc->config.executable);
    SetEvent(gc->hook_stop);
    return true;
  }

  return false;
}

static bool init_hook(struct game_capture *gc)
{
  struct dstr exe = { 0 };
  bool blacklisted_process = false;

  if (0 && gc->config.mode == CAPTURE_MODE_ANY) {
    if (get_window_exe(&exe, gc->next_window)) {
      info("attempting to hook fullscreen process: %s", exe.array);
    }
  } else {
    info("attempting to hook process: %s", gc->executable.array);
    dstr_copy_dstr(&exe, &gc->executable);
  }

  blacklisted_process = is_blacklisted_exe(exe.array);
  if (blacklisted_process)
    info("cannot capture %s due to being blacklisted", exe.array);
  dstr_free(&exe);

  if (blacklisted_process) {
    return false;
  }
  if (target_suspended(gc)) {
    info("target is suspended");
    return false;
  }
  if (!open_target_process(gc)) {
    return false;
  }
  if (!init_keepalive(gc)) {
    return false;
  }
  if (!init_pipe(gc)) {
    return false;
  }
  if (!attempt_existing_hook(gc)) {
    if (!inject_hook(gc)) {
      return false;
    }
  }
  if (!init_texture_mutexes(gc)) {
    return false;
  }

  if (!init_hook_info(gc)) {
    return false;
  }

  if (!init_events(gc)) {
    return false;
  }

  SetEvent(gc->hook_init);

  gc->window = gc->next_window;
  gc->next_window = NULL;
  gc->active = true;
  gc->retrying = 0;
  return true;
}

static void stop_capture(struct game_capture *gc)
{
  ipc_pipe_server_free(&gc->pipe);

  if (gc->hook_stop) {
    SetEvent(gc->hook_stop);
  }

  if (gc->global_hook_info) {
    UnmapViewOfFile(gc->global_hook_info);
    gc->global_hook_info = NULL;
  }

  if (gc->data) {
    UnmapViewOfFile(gc->data);
    gc->data = NULL;
  }

  if (gc->app_sid) {
    LocalFree(gc->app_sid);
    gc->app_sid = NULL;
  }

  close_handle(&gc->hook_restart);
  close_handle(&gc->hook_stop);
  close_handle(&gc->hook_ready);
  close_handle(&gc->hook_exit);
  close_handle(&gc->hook_init);
  close_handle(&gc->hook_data_map);
  close_handle(&gc->keepalive_mutex);
  close_handle(&gc->global_hook_info_map);
  close_handle(&gc->target_process);
  close_handle(&gc->texture_mutexes[0]);
  close_handle(&gc->texture_mutexes[1]);

  if (gc->active) {
    info("game capture stopped");
  }

  gc->wait_for_target_startup = false;
  gc->active = false;
  gc->capturing = false;

  if (gc->retrying)
    gc->retrying--;
}


static void try_hook(struct game_capture *gc)
{
  if (0 && gc->config.mode == CAPTURE_MODE_ANY) {
    get_fullscreen_window(gc);
  } else {
    get_selected_window(gc);
  }

  if (gc->next_window) {
    gc->thread_id = GetWindowThreadProcessId(gc->next_window, &gc->process_id);

    // Make sure we never try to hook ourselves (projector)
    if (gc->process_id == GetCurrentProcessId())
      return;

    if (!gc->thread_id && gc->process_id)
      return;

    if (!gc->process_id) {
      warn("error acquiring, failed to get window thread/process ids: %lu",
          GetLastError());
      gc->error_acquiring = true;
      return;
    }

    if (!init_hook(gc)) {
      stop_capture(gc);
    }
  } else {
    gc->active = false;
  }
}

gboolean game_capture_is_ready(void * data) {
  if (data == NULL) {
    return FALSE;
  }

  struct game_capture *gc = (game_capture *)data;
  return gc->active && !gc->retrying;
}

gboolean game_capture_is_active(void * data) {
  if (data == NULL) {
    return FALSE;
  }

  struct game_capture *gc = (game_capture *)data;
  return gc->active;
}

void set_fps(void **data, uint64_t frame_interval) {
  struct game_capture *gc = (game_capture *)*data;

  if (gc == NULL) {
    debug("set_fps: gc==NULL");
    return;
  }

  debug("set_fps: %d", frame_interval);
  gc->global_hook_info->frame_interval = frame_interval;
}

void* game_capture_start(void **data, 
    char* window_class_name_c, char* window_name_c,
    GameCaptureConfig *config, uint64_t frame_interval) {
  struct game_capture *gc = (game_capture *)*data;

  if (gc == NULL) {
    HWND hwnd = NULL;
    window_priority priority = WINDOW_PRIORITY_EXE;

    wchar_t* window_class_name = get_wc(window_class_name_c);
    wchar_t* window_name = get_wc(window_name_c);

    if (lstrlenW(window_class_name) > 0 &&
        lstrlenW(window_name) > 0) {
      hwnd = FindWindowW(window_class_name, window_name);
    } else if (lstrlenW(window_class_name) > 0) {
      hwnd = FindWindowW(window_class_name, NULL);
      priority = WINDOW_PRIORITY_CLASS;
    } else if (lstrlenW(window_name) > 0) {
      hwnd = FindWindowW(NULL, window_name);
      priority = WINDOW_PRIORITY_TITLE;
    }

    delete[] window_class_name;
    delete[] window_name;

    if (hwnd == NULL) {
      return NULL;
    }

    config->window = hwnd;

    gc = game_capture_create(config, frame_interval);

    struct dstr *klass = &gc->klass;
    struct dstr *title = &gc->title;
    struct dstr *exe = &gc->executable;
    get_window_class(klass, hwnd);
    get_window_exe(exe, hwnd);
    get_window_title(title, hwnd);

    gc->config.executable = _strdup(exe->array);
    gc->config.title = _strdup(title->array);
    gc->config.klass = _strdup(klass->array);;
    gc->priority = priority;
  }

  uint64_t current_time_ns = os_gettime_ns();
  if (current_time_ns < gc->next_retry_time_ns) {
    return gc;
  }

  try_hook(gc);

  if (!gc->active || !gc->retrying) {
    gc->next_retry_time_ns = get_next_retry_time(5000); // 5 seconds
  }

  return gc;
}

enum capture_result {
  CAPTURE_FAIL,
  CAPTURE_RETRY,
  CAPTURE_SUCCESS
};

static inline enum capture_result init_capture_data(struct game_capture *gc)
{
  gc->cx = gc->global_hook_info->cx;
  gc->cy = gc->global_hook_info->cy;
  gc->pitch = gc->global_hook_info->pitch;

  if (gc->data) {
    UnmapViewOfFile(gc->data);
    gc->data = NULL;
  }

  CloseHandle(gc->hook_data_map);

  gc->hook_data_map = open_map_plus_id(gc, SHMEM_TEXTURE,
      gc->global_hook_info->map_id);
  if (!gc->hook_data_map) {
    DWORD error = GetLastError();
    if (error == 2) {
      return CAPTURE_RETRY;
    } else {
      warn("init_capture_data: failed to open file "
          "mapping: %lu", error);
    }
    return CAPTURE_FAIL;
  }

  gc->data = MapViewOfFile(gc->hook_data_map, FILE_MAP_ALL_ACCESS, 0, 0,
      gc->global_hook_info->map_size);
  if (!gc->data) {
    warn("init_capture_data: failed to map data view: %lu",
        GetLastError());
    return CAPTURE_FAIL;
  }

  info("init_capture_data successful for %s, %s, %s", 
      gc->config.title, gc->config.klass, gc->config.executable);
  return CAPTURE_SUCCESS;
}

static inline bool init_shmem_capture(struct game_capture *gc)
{
  gc->texture_buffers[0] = (uint8_t*)gc->data + gc->shmem_data->tex1_offset;
  gc->texture_buffers[1] = (uint8_t*)gc->data + gc->shmem_data->tex2_offset;
  gc->convert_16bit = NULL; //is_16bit_format(gc->global_hook_info->format);
  return true;
}

static inline bool init_shtex_capture(struct game_capture *gc)
{
  return true;
}

static bool start_capture(struct game_capture *gc)
{
  if (gc->global_hook_info->type == CAPTURE_TYPE_MEMORY) {
    if (!init_shmem_capture(gc)) {
      return false;
    }

  } else {
    if (!init_shtex_capture(gc)) {
      return false;
    }
  }

  return true;
}

static inline bool capture_valid(struct game_capture *gc)
{
  if (!gc->dwm_capture && !IsWindow(gc->window))
    return false;

  return !object_signalled(gc->target_process);
}

gboolean game_capture_tick(void * data) {
  struct game_capture *gc = (game_capture *) data;

  if (!gc->active) {
    return FALSE;
  }

  if (gc->active && !gc->hook_ready && gc->process_id) {
    debug("re-subscribing to hook_ready");
    gc->hook_ready = open_event_gc(gc, EVENT_HOOK_READY);
  }

  if (gc->hook_ready && object_signalled(gc->hook_ready)) {
    debug("capture initializing!");
    enum capture_result result = init_capture_data(gc);

    if (result == CAPTURE_SUCCESS)
      gc->capturing = start_capture(gc);
    else
      info("init_capture_data failed");
  }

  if (gc->active) {
    if (!capture_valid(gc)) {
      info("capture window no longer exists, "
          "terminating capture");
      stop_capture(gc);
      g_free(gc);
      return FALSE;
    }
  }

  return TRUE;
}

gboolean game_capture_stop(void *data) {
  struct game_capture *gc = (game_capture*) data;
  stop_capture(gc);
  g_free(gc);
  return TRUE;
}

wchar_t *get_wc(const char *c) {
  int wc_size = MultiByteToWideChar(CP_UTF8, 0, c, -1, NULL, 0);
  wchar_t* wc = new wchar_t[wc_size];
  MultiByteToWideChar(CP_UTF8, 0, c, -1, wc, wc_size);
  return wc;
}

gboolean game_capture_shmem_draw_frame(struct game_capture* gc, uint8_t* dst_data, uint32_t dst_stride) {
  HANDLE mutex;
  int cur_texture;
  int next_texture;

  if (!gc->shmem_data) {
    return FALSE;
  }

  cur_texture = gc->shmem_data->last_tex;
  if (cur_texture < 0 || cur_texture > 1)
    return false;

  next_texture = cur_texture == 1 ? 0 : 1;
  if (object_signalled(gc->texture_mutexes[cur_texture])) {
    mutex = gc->texture_mutexes[cur_texture];
  } else if (object_signalled(gc->texture_mutexes[next_texture])) {
    mutex = gc->texture_mutexes[next_texture];
    cur_texture = next_texture;
  } else {
    return false;
  }

  if (gc->convert_16bit) {
    error("copy_shmem_text 16 bit - not handled");
    // copy_16bit_tex(gc, cur_texture, pData, pitch);
  } else {
    const uint8* src_frame = gc->texture_buffers[cur_texture];
    int src_stride_frame = gc->pitch;
    int width = gc->cx;
    int height = gc->cy;

    if (gc->global_hook_info->flip) {
      height = -height;
    }

    int err = 0;
    if (gc->global_hook_info->format == DXGI_FORMAT_R8G8B8A8_UNORM) {
      // Overwatch
      // ABGR -> ABGR
      err = libyuv::ARGBCopy(src_frame,
          src_stride_frame,
          dst_data,
          dst_stride,
          width,
          height);
    } else if (gc->global_hook_info->format == DXGI_FORMAT_B8G8R8A8_UNORM) {
      // Hearthstone
      // opengl / minecraft (javaw.exe)
      err = libyuv::ARGBToABGR(src_frame,
          src_stride_frame,
          dst_data,
          dst_stride,
          width,
          height);
    } else if (gc->global_hook_info->format == DXGI_FORMAT_B8G8R8X8_UNORM) {
      // League Of Legends 7.2.17
      err = libyuv::ARGBToABGR(src_frame,
          src_stride_frame,
          dst_data,
          dst_stride,
          width,
          height);
    } else if (gc->global_hook_info->format == DXGI_FORMAT_R10G10B10A2_UNORM) {
      error("Unknown DXGI FORMAT %d", gc->global_hook_info->format);
    } else {
      error("Unknown DXGI FORMAT %d", gc->global_hook_info->format);
    }

    if (err) {
      error("yuv conversion failed");
    }
  }

  ReleaseMutex(mutex);
  return TRUE;
}
