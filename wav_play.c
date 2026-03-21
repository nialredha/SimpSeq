//
// References
//   https://medium.com/@shahidahmadkhan86/sound-in-windows-the-wasapi-in-c-23024cdac7c6 
//   https://www.reddit.com/r/C_Programming/comments/1gv80uq/cannot_solve_errors_for_unresolved_external/
//   https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream
//

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

typedef struct
{
    Wav_Format format;
    u32        playhead;
    u32        size;
    u8*        data;
} User_Data;

void audio_callback(void* user_data, u32 num_frames_needed, void* output)
{
    User_Data* ud = (User_Data*)user_data;

    u8* src  = (u8*)&ud->data[ud->playhead];
    u8* dest = (u8*)output;

    u32 num_bytes_needed = (num_frames_needed * ud->format.num_channels) * (ud->format.bits_per_sample / 8);

    for (u32 byte_index = 0; byte_index < num_bytes_needed; ++byte_index)
    {
        if (ud->playhead + byte_index < ud->size) 
        {
            *dest++ = *src++;
            ud->playhead += sizeof(*dest);
        }
        else
        {
            *dest++ = 0;
        }
    }
}

s32 main2(s32 arg_count, char** args)
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

    User_Data    user_data = { wav_fmt, 0, wav_data_node.header.size, (u8*)&wav_file.data[wav_data_node.data_offset] };
    OS_Audio_Config config = { wav_fmt, audio_callback, &user_data };

    HRESULT result = 0;
    result = os_win32_audio_init(&device, &config);
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

    while (user_data.playhead < user_data.size) 
    {
        // do nothing
        os_sleep_ms(100);
    }

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
