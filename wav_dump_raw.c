#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "base_io.h"
#include "wav.h"
#include "wav_dump_utils.h"

#include "base_arena.c"
#include "base_string.c"
#include "base_io.c"
#include "wav.c"
#include "wav_dump_utils.c"

int main(int argc, char** argv)
{
    // globals
    Arena arena = {0};

    String filename_in  = {0};
    String file_in      = {0};

    String filename_out = {0};

    // get input filename
    if (argc < 2)
    {
        fprintf(stderr, "ERROR: not enough arguments!\n  Usage: .\\%s <path_to_wav_file> <OPTIONAL: path_to_output_dump_file>\n", argv[0]);
        return 1;
    }
    filename_in = STR_C(argv[1]);

    // try get optional output filename, default to stdout
    if (argc > 2)
    {
        if (argc != 3)
        {
            fprintf(stderr, "ERROR: too many arguments!\n  Usage: .\\%s <path_to_wav_file> <OPTIONAL: path_to_output_dump_file>\n", argv[0]);
            return 1;
        }
        filename_out = STR_C(argv[2]);
    }

    arena_alloc(&arena, 4096);

    file_in = read_entire_file(filename_in);
    if (file_in.data == 0)
    {
        fprintf(stderr, "Failed to read file %.*s!\n", filename_in.count, filename_in.data);
        return 1;
    }

    String_Builder strb = {0};
    arena_alloc(&strb.arena, 5*file_in.count);

    WAV_DUMP_BIN(&arena, (u8*)file_in.data, file_in.count, &strb);

    // output to stream
    FILE* stream = stdout;
    if (filename_out.data != 0)
    {
        stream = fopen(filename_out.data, "wb");
    }
    else
    {
        filename_out = STR_LIT("stdout");
    }

    u32 bytes_written = (u32)fwrite(strb.str.data, 1, strb.str.count, stream);
    if (bytes_written != strb.str.count)
    {
        fprintf(stderr, "Failed to dump %s to %s!\n", filename_in.data, filename_out.data);
        return 1;
    }

    printf("\n");

    u32 left = arena.size - arena.used;
    printf("Main Arena Metrics:\n");
    printf("  Size: %4u B | %.2f MB | %.2f GB\n", arena.size, (f32)arena.size/(1024.0f*1024.0f), (f32)arena.size/(1024.0f*1024.0f*1024.0f));
    printf("  Used: %4u B | %.2f MB | %.2f GB\n", arena.used, (f32)arena.used/(1024.0f*1024.0f), (f32)arena.used/(1024.0f*1024.0f*1024.0f));
    printf("  Left: %4u B | %.2f MB | %.2f GB\n", left,       (f32)left/      (1024.0f*1024.0f), (f32)left      /(1024.0f*1024.0f*1024.0f));
    printf("\n");

    left = strb.arena.size - strb.arena.used;
    printf("STRB Arena Metric:\n");
    printf("  Size: %10u B | %7.2f MB | %4.2f GB\n", strb.arena.size, (f32)strb.arena.size/(1024.0f*1024.0f), (f32)strb.arena.size/(1024.0f*1024.0f*1024.0f));
    printf("  Used: %10u B | %7.2f MB | %4.2f GB\n", strb.arena.used, (f32)strb.arena.used/(1024.0f*1024.0f), (f32)strb.arena.used/(1024.0f*1024.0f*1024.0f));
    printf("  Left: %10u B | %7.2f MB | %4.2f GB\n", left,             (f32)left            /(1024.0f*1024.0f), (f32)left            /(1024.0f*1024.0f*1024.0f));
    printf("\n");

    f32 chars_per_byte = (f32)strb.arena.used / (f32)file_in.count;
    printf("Chars per Byte: %f\n", chars_per_byte);

    return 0;
}
