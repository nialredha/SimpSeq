#ifndef OS_WIN32_H
#define OS_WIN32_H

#include <windows.h>

//
// DLL
//

typedef struct 
{
    String      filename;
    HMODULE     handle;
    FILETIME    last_write_time;
} OS_Win32_DLL;

static bool  os_win32_reload_dll(OS_Win32_DLL* dll);
static void  os_win32_unload_dll(OS_Win32_DLL* dll);

static void* os_win32_get_function_pointer(OS_Win32_DLL* dll, String function_name);

//
// TIME
//

FILETIME      os_win32_get_last_write_time_of_file      (String filename);
f32           os_win32_get_seconds_elapsed              (LARGE_INTEGER start, LARGE_INTEGER end, s64 perf_counter_freq);
LARGE_INTEGER os_win32_get_performance_counter_count    (void);
LARGE_INTEGER os_win32_get_performance_counter_frequency(void);

//
// INTRINSICS
//

#include <winnt.h>

inline s32 os_win32_atomic_add_s32(LONG* volatile value, s32 addend)
{
    s32 result = _InterlockedExchangeAdd(value, addend);
    return result;
}

inline s64 os_win32_atomic_add_s64(s64* volatile value, s64 addend)
{
    s64 result = _InterlockedExchangeAdd64(value, addend);
    return result;
}

//
// WASAPI
//
//   References:
//     https://medium.com/@shahidahmadkhan86/sound-in-windows-the-wasapi-in-c-23024cdac7c6 
//     https://www.reddit.com/r/C_Programming/comments/1gv80uq/cannot_solve_errors_for_unresolved_external/
//     https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream
//

#define INITGUID
#include <initguid.h>
#include <mmdeviceapi.h> // enumerate/activate audio endpoints

#include <audioclient.h> // WASAPI interfaces

// BCDE0395-E52F-467C-8E3D-C4579291692E
DEFINE_GUID(IID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
// A95664D2-9614-4F35-A746-DE8DB63617E6
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
// 1CB9AD4C-DBFA-4C32-B178-C2F568A703B2 
DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
// F294ACFC-3146-4483-A7BF-ADDCA7C260E2
DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);

#define REF_TIME_NANOSECONDS_PER_UNIT   (100.0)
#define REF_TIME_UNITS_PER_NANOSECOND   ( 1.0 / REF_TIME_NANOSECONDS_PER_UNIT)
#define REF_TIME_NANOSECONDS_PER_SECOND (1e9)
#define REF_TIME_UNITS_PER_SECOND       (REF_TIME_NANOSECONDS_PER_SECOND * REF_TIME_UNITS_PER_NANOSECOND)

typedef void (* Audio_Callback)(Wav_Format* format, void* user_data, u32 num_frames_needed, void* output); 

typedef struct
{
    Wav_Format     desired_format;
    Audio_Callback audio_callback;
    void*          user_data;
} OS_Audio_Config;

typedef struct
{
    IAudioClient*       audio_client;
    WAVEFORMATEX*       audio_format;
    IAudioRenderClient* render_client;
    HANDLE              event_handle;

    Audio_Callback audio_callback;
    void*          user_data;

    HANDLE audio_thread;
    bool   quit;
} OS_Audio_Device;

s32 os_win32_audio_init(OS_Audio_Device* device, OS_Audio_Config* config);
s32 os_win32_audio_start(OS_Audio_Device* device);
s32 os_win32_audio_stop(OS_Audio_Device* device);
void os_win32_audio_deinit(OS_Audio_Device* device);

#endif // OS_WIN32_H
