#include "core.h"
#include "wav.h"
#include "wav_dump_utils.h"
#include "os_win32.h"

#include "core.c"
#include "wav.c"
#include "wav_dump_utils.c"
#include "os_win32.c"

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
    if (arg_count != 2)
    {
        printf("%d is not enough arguments!\n Example usage: .\\%s test.wav\n", arg_count, args[0]);
        return 1;
    }

    // WAV globals
    Arena  wav_arena = {0};
    arena_alloc(&wav_arena, 4096);

    char* filename_cstr8 = args[1];
    String wav_filename  = STR_C(filename_cstr8);
    String wav_file      = os_read_entire_file(wav_filename);

    Wav wav = wav_from_data(&wav_arena, wav_file);

    Wav_Sub_Chunk_Node wav_fmt_node  = wav_sub_chunk_from_id(&wav.sub_chunks, WAV_FOURCC(WAV_FORMAT_CHUNK_ID));
    Wav_Sub_Chunk_Node wav_data_node = wav_sub_chunk_from_id(&wav.sub_chunks, WAV_FOURCC(WAV_DATA_CHUNK_ID));

    Wav_Format wav_fmt  = *(Wav_Format*)&wav_file.data[wav_fmt_node.data_offset];

    // TODO[nr] @ugly
    String_Builder strb = {0};
    arena_alloc(&strb.arena, 1024);
    wav_dump_format(&wav_arena, &wav_fmt, 1, &strb);
    printf("Input File %s:\n%.*s\n", wav_filename.data, strb.str.count, strb.str.data);
    arena_free(&strb.arena);

    OS_Audio_Device device = {0};

    f32 seconds_per_loop = 0.1f;
    u32 samples_per_loop = (u32)((seconds_per_loop * (f32)wav_fmt.sample_rate) * (f32)wav_fmt.num_channels);
    u32 bytes_per_loop   = samples_per_loop * (wav_fmt.bits_per_sample / 8);
    printf("bytes_per_loop: %u\n", bytes_per_loop);

    Ring_Buffer rb = rb_create(bytes_per_loop*2);

    User_Data    user_data = { &rb };
    OS_Audio_Config config = { wav_fmt, audio_callback, &user_data };

    HRESULT result = 0;
    result = os_win32_audio_init(&device, &config);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to init audio client!\n");
        return result;
    }

    u8* src          = (u8*)&wav_file.data[wav_data_node.data_offset];
    u32 src_size     = wav_data_node.header.size;
    u32 src_playhead = 0;

    // preload
    {
        u32 bytes_written = rb_write(&rb, &src[src_playhead], bytes_per_loop);
        src_playhead += bytes_written;
    }

    result = os_win32_audio_start(&device);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to start audio client!\n");
        return result;
    }
    
    while (src_playhead < src_size) 
    {
        u32 bytes_to_write = 0; 

        if (src_playhead + bytes_per_loop < src_size)
        {
            bytes_to_write = bytes_per_loop;
        }
        else
        {
            bytes_to_write = src_size - src_playhead;
        }

        u32 bytes_written = rb_write(&rb, &src[src_playhead], bytes_to_write);
        src_playhead += bytes_written;
    }

    s32 cap = rb.capacity;
    do 
    { 
        os_win32_atomic_compare_exchange_s32((s32 volatile *)&cap, 0, (s32)rb.amount_free); 
        os_sleep_ms(100);
    } while ((u32)cap == rb.capacity);

    // stop playing device
    result = os_win32_audio_stop(&device);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to stop audio client!\n");
        return result;
    }

    os_win32_audio_deinit(&device);

    return 0;
}
