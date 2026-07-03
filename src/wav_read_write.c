#include "core.h"
#include "wav.h"
#include "wav_dump_utils.h"
#include "os_win32.h"

#include "core.c"
#include "wav.c"
#include "wav_dump_utils.c"
#include "os_win32.c"

int entry_point(int argc, char** argv)
{
    // globals
    Arena arena = {0};

    String filename_in = {0};
    String file_in     = {0};

    String filename_out = {0};

    // get input filename
    if (argc < 2)
    {
        fprintf(stderr, "ERROR: not enough arguments!\n  Usage: .\\%s <path_to_wav_file> <OPTIONAL: path_to_output_wav_file>\n", argv[0]);
        return 1;
    }
    filename_in = STR_C(argv[1]);

    // try get optional output filename, default to stdout
    if (argc > 2)
    {
        if (argc != 3)
        {
            fprintf(stderr, "ERROR: too many arguments!\n  Usage: .\\%s <path_to_wav_file> <OPTIONAL: path_to_output_wav_file>\n", argv[0]);
            return 1;
        }
        filename_out = STR_C(argv[2]);
    }

    arena_alloc(&arena, 1024*1024);

    file_in = os_read_entire_file(filename_in);
    if (file_in.data == 0)
    {
        fprintf(stderr, "Failed to read file %.*s!\n", filename_in.count, filename_in.data);
        return 1;
    }

    Wav        wav  = wav_from_data(&arena, file_in);
    Wav_Format fmt  = wav_get_format(&wav.sub_chunks, file_in);
    Wav_Data   data = wav_get_data(&arena, &wav.sub_chunks, file_in);


    Wav_Render_Params params = {
        .num_channels      = fmt.num_channels,
        .bytes_per_sample  = fmt.bits_per_sample / 8,
        .sample_rate       = fmt.sample_rate
    };
    Wav_Data new_data = wav_render(&arena, params, data);

    if (filename_out.data == 0)
    {
        filename_out = STR_LIT("output.wav");
    }

    String contents = { .data = (char*)new_data.buffer, .count = new_data.size };

    if (os_write_entire_file(filename_out, contents))
    {
        printf("Success!\n");
    }
 
    return 0;
}
