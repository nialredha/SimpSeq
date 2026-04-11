#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "base_ring_buffer.h"

#include "wav.h"
#include "wav_dump_utils.h"

#include "os.h"
#include "os_win32.h"

#include "base_arena.c"
#include "base_string.c"
#include "base_ring_buffer.c"

#include "wav.c"
#include "wav_dump_utils.c"

#include "os.c"
#include "os_win32.c"

#include "simp_seq.h"

typedef void (SS_Post_Load)(Simp_Seq_State* state);
typedef void (SS_Post_Reload)(Simp_Seq_State* state);

typedef void (SS_Update)(Simp_Seq_State* state, Ring_Buffer* output);

typedef struct
{
    Ring_Buffer* rb;
} User_Data;

void audio_callback(Wav_Format* format, void* user_data, u32 num_frames_needed, void* output)
{
    User_Data*   ud  = (User_Data*)user_data;
    Ring_Buffer* rb  = ud->rb;

    u32 bytes_needed  = (num_frames_needed * format->num_channels) * (format->bits_per_sample / 8);
    rb_read(rb, (u8*)output, bytes_needed);
}

s32 entry_point(s32 arg_count, char** args)
{
    (void)arg_count;
    (void)args;

    OS_Win32_DLL ss_dll = {0};
    ss_dll.filename = STR_LIT("ss.dll");

    Simp_Seq_State ss_state  = {0};

    SS_Post_Load*   ss_post_load   = 0;
    SS_Post_Reload* ss_post_reload = 0;
    SS_Update*      ss_update      = 0;

    Ring_Buffer output_buffer = rb_create(9600*4); // TODO[nr] @dobetter

    OS_Audio_Device device = {0};

    Wav_Format desired_format = {0};
    desired_format.format_tag      = Wav_Format_Tag_IEE_FLOAT;
    desired_format.num_channels    = 2;
    desired_format.sample_rate     = 48000;
    desired_format.bits_per_sample = 32;

    User_Data user_data = { &output_buffer };

    OS_Audio_Config config = { desired_format, audio_callback, &user_data };

    s32 result = os_win32_audio_init(&device, &config);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to init audio client!\n");
        return result;
    }

    result = os_win32_audio_start(&device);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to start audio client!\n");
        return result;
    }

    os_win32_reload_dll(&ss_dll);

    ss_post_load   = (SS_Post_Load*)os_win32_get_function_pointer(&ss_dll, STR_LIT("ss_post_load"));
    ss_post_reload = (SS_Post_Reload*)os_win32_get_function_pointer(&ss_dll, STR_LIT("ss_post_reload"));
    ss_update      = (SS_Update*)os_win32_get_function_pointer(&ss_dll, STR_LIT("ss_update"));

    if (ss_post_load)   { ss_post_load(&ss_state);   }
    if (ss_post_reload) { ss_post_reload(&ss_state); }

    while (true)
    {
        FILETIME current_write_time = os_win32_get_last_write_time_of_file(ss_dll.filename);

        // TODO[nr] @move: to win32 layer
        if (CompareFileTime(&ss_dll.last_write_time, &current_write_time) != 0)
        {
            printf("\nDetected change! Reloading %s\n", ss_dll.filename.data);
            SYSTEMTIME last_sys_time;
            SYSTEMTIME curr_sys_time;
            FileTimeToSystemTime(&ss_dll.last_write_time, &last_sys_time);
            FileTimeToSystemTime(&current_write_time, &curr_sys_time);
            printf("  Last: %02uh:%02um:%02us:%04ums\n", last_sys_time.wHour, last_sys_time.wMinute, last_sys_time.wSecond, last_sys_time.wMilliseconds);
            printf("  Curr: %02uh:%02um:%02us:%04ums\n", curr_sys_time.wHour, curr_sys_time.wMinute, curr_sys_time.wSecond, curr_sys_time.wMilliseconds);

            if (os_win32_reload_dll(&ss_dll))
            {
                printf("Reloaded %s!\n", ss_dll.filename.data);

                ss_post_reload = (SS_Post_Reload*)os_win32_get_function_pointer(&ss_dll, STR_LIT("ss_post_reload"));
                if (ss_post_reload) { ss_post_reload(&ss_state); }
            }

            ss_update = (SS_Update*)os_win32_get_function_pointer(&ss_dll, STR_LIT("ss_update"));
        }

        if (ss_update != 0)
        {
            ss_update(&ss_state, &output_buffer);
        }

        os_sleep_ms(10);
    }

    // stop playing device
    result = os_win32_audio_stop(&device);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to stop audio client!\n");
        return result;
    }

    os_win32_audio_deinit(&device);
}
