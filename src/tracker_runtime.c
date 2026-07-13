#include "core.h"
#include "wav.h"
#include "wav_dump_utils.h"
#include "os_win32.h"

#include "fx.h"
#include "tracker.h"
#include "tracker_module.h"

#include "core.c"
#include "wav.c"
#include "wav_dump_utils.c"
#include "os_win32.c"

typedef void (Trk_Module_Post_Load)(Trk* trk);
typedef void (Trk_Module_Post_Reload)(Trk* trk);

typedef void (Trk_Module_Update)(Trk* trk, Ring_Buffer* out);

typedef struct
{
    Ring_Buffer* rb;
} User_Data;

void audio_callback(Wav_Format* format, void* user_data, u32 num_frames_needed, void* output)
{
    User_Data*   ud  = (User_Data*)user_data;
    Ring_Buffer* rb  = ud->rb;

    u32 bytes_needed = (num_frames_needed * format->num_channels) * (format->bits_per_sample / 8);
    u32 amount_read  = rb_read(rb, (u8*)output, bytes_needed);

    if (amount_read < bytes_needed)
    {
        u8* dest = (u8*)output + amount_read;
        for (u32 i = 0; i < bytes_needed - amount_read; ++i)
        {
            dest[i] = 0;
        }
    }
}

s32 entry_point(s32 arg_count, char** args)
{
    (void)arg_count;
    (void)args;

    OS_Win32_DLL trk_module_dll = {0};
    trk_module_dll.filename = STR_LIT("trk_module.dll");

    Trk trk = {0};

    Trk_Module_Post_Load*   trk_module_post_load   = 0;
    Trk_Module_Post_Reload* trk_module_post_reload = 0;
    Trk_Module_Update*      trk_module_update      = 0;

    Ring_Buffer rb_line_out = rb_create(9600*4); // TODO[nr] @dobetter
    Ring_Buffer rb_file     = {0};

    Ring_Buffer* rb = &rb_line_out;

    os_win32_reload_dll(&trk_module_dll);

    trk_module_post_load   = (Trk_Module_Post_Load*)os_win32_get_function_pointer(&trk_module_dll, STR_LIT("trk_module_post_load"));
    trk_module_post_reload = (Trk_Module_Post_Reload*)os_win32_get_function_pointer(&trk_module_dll, STR_LIT("trk_module_post_reload"));
    trk_module_update      = (Trk_Module_Update*)os_win32_get_function_pointer(&trk_module_dll, STR_LIT("trk_module_update"));

    if (trk_module_post_load)   { trk_module_post_load(&trk);   }
    if (trk_module_post_reload) { trk_module_post_reload(&trk); }

    // audio init
    OS_Audio_Device device = {0};
    {
        Wav_Format desired_format = {0};
        desired_format.format_tag      = Wav_Format_Tag_IEE_FLOAT;
        desired_format.num_channels    = 2;
        desired_format.sample_rate     = 48000;
        desired_format.bits_per_sample = 32;

        User_Data user_data = { &rb_line_out };

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
    }

    Arena file_arena = {0};
    String render_filename = STR_LIT("render.wav");

    if (trk.render_to_file)
    {
        arena_alloc(&file_arena, 1024*1024);

        rb_file = rb_create(9600*4); // TODO[nr] @dobetter

        Wav_Render_Params params = {
            .num_channels = SUPPORTED_CHANNEL_COUNT,
            .bytes_per_sample = SUPPORTED_BIT_DEPTH / 8,
            .sample_rate = SUPPORTED_SAMPLE_RATE
        };

        // write valid WAV file with no data (yet)
        if (!wav_render_entire_file(&file_arena, params, (Wav_Data){0}, render_filename))
        {
            printf("Failed to write empty WAV file %s!!!\n", render_filename.data);
        }
        else
        {
            printf("Wrote empty WAV file %s!!!\n", render_filename.data);
        }


        rb = &rb_file;
    }

    bool reload = false;

    while (true)
    {
        FILETIME current_write_time = os_win32_get_last_write_time_of_file(trk_module_dll.filename);

        // TODO[nr] @move: to win32 layer
        if (CompareFileTime(&trk_module_dll.last_write_time, &current_write_time) != 0 || reload)
        {
            reload = true;

#if 0
            printf("\nDetected change! Reloading %s\n", trk_module_dll.filename.data);
            SYSTEMTIME last_sys_time;
            SYSTEMTIME curr_sys_time;
            FileTimeToSystemTime(&trk_module_dll.last_write_time, &last_sys_time);
            FileTimeToSystemTime(&current_write_time, &curr_sys_time);
            printf("  Last: %02uh:%02um:%02us:%04ums\n", last_sys_time.wHour, last_sys_time.wMinute, last_sys_time.wSecond, last_sys_time.wMilliseconds);
            printf("  Curr: %02uh:%02um:%02us:%04ums\n", curr_sys_time.wHour, curr_sys_time.wMinute, curr_sys_time.wSecond, curr_sys_time.wMilliseconds);
#endif

            if (os_win32_reload_dll(&trk_module_dll))
            {
                printf("\nReloaded %s!\n", trk_module_dll.filename.data);

                trk_module_post_reload = (Trk_Module_Post_Reload*)os_win32_get_function_pointer(&trk_module_dll, STR_LIT("trk_module_post_reload"));
                if (trk_module_post_reload) { trk_module_post_reload(&trk); }

                reload = false;
            }

            trk_module_update = (Trk_Module_Update*)os_win32_get_function_pointer(&trk_module_dll, STR_LIT("trk_module_update"));
        }

        if (trk_module_update != 0)
        {
            trk_module_update(&trk, rb);
        }

        if (trk.render_to_file)
        {
            u8 temp_buffer[9600*4] = {0};

            u32 bytes_to_read = rb_write_space(&rb_line_out);
            u32 bytes_read    = rb_read(rb, temp_buffer, bytes_to_read);

            // stream data into file
            {
                wav_render_append_data_to_file(&file_arena, render_filename, (Wav_Data){ .buffer=temp_buffer, .size=bytes_read});
                u32 bytes_written = rb_write(&rb_line_out, temp_buffer, bytes_read);
                assert(bytes_read == bytes_written);
            }

        }

        os_sleep_ms(10);
    }

    // stop playing device
    {
        s32 result = os_win32_audio_stop(&device);
        if (result < 0)
        {
            fprintf(stderr, "ERROR: failed to stop audio client!\n");
            return result;
        }
    }

    os_win32_audio_deinit(&device);
}
