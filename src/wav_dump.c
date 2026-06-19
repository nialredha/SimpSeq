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

    Wav wav = {0};

    String_Builder strb = {0};

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

    file_in = os_read_entire_file(filename_in);
    if (file_in.data == 0)
    {
        fprintf(stderr, "Failed to read file %.*s!\n", filename_in.count, filename_in.data);
        return 1;
    }

    wav = wav_from_data(&arena, file_in);

    arena_alloc(&strb.arena, 1024);

    // riff chunk
    wav_dump_riff(&arena, &wav.riff_chunk, 0, &strb);

    // sub chunks
    for (Wav_Sub_Chunk_Node* n = wav.sub_chunks.first; n != 0; n = n->next)
    {
        if (n->header.id == WAV_FOURCC(WAV_FORMAT_CHUNK_ID))
        {
            Wav_Format* format = (Wav_Format*)(file_in.data + n->data_offset);

            String fmt_id  = WAV_STR_FROM_FOURCC(n->header.id);

            strb_append(&strb, 
                        str_format(&arena, 
                                   "Sub-Chunk %.*s {\n"
                                   "  id            = %.*s,\n" 
                                   "  size          = %u,\n",
                                   fmt_id.count, fmt_id.data,
                                   fmt_id.count, fmt_id.data,
                                   n->header.size));

            wav_dump_format(&arena, format, 1, &strb);
        }
        else
        {
            wav_dump_sub_chunk(&arena, n, 0, &strb);
        }
    }

    //
    // TODO[nr] @fix: add the following functionality to win32 os layer
    //

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


