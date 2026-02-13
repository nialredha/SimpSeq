#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "base_io.h"
#include "wav.h"

#include "base_arena.c"
#include "base_string.c"
#include "base_io.c"
#include "wav.c"

int main(int args, char** argv)
{
    if (args != 2)
    {
        printf("Not enough arguments!\n Example usage: .\\%s test.wav\n", argv[0]);
        return 1;
    }

    Arena arena = {0};
    arena_alloc(&arena, 4096);

    String filename   = STR_C(argv[1]);
    String file       = read_entire_file(filename);
    Chunk_Node* first = wav_chunks_from_file(&arena, file);

    RIFF_Chunk* riff_chunk = (RIFF_Chunk*)first->header;
    if (riff_chunk->header.id == FOURCC(RIFF_CHUNK_ID) && 
        riff_chunk->format    == FOURCC(RIFF_CHUNK_FORMAT_WAV))
    {
        String riff_chunk_id = STR_FROM_FOURCC(riff_chunk->header.id);
        String riff_format   = STR_FROM_FOURCC(riff_chunk->format);

        printf("Chunk %.*s {\n"
               "  id     = %.*s,\n"
               "  size   = %u,\n"
               "  format = %.*s\n",
               riff_chunk_id.count, riff_chunk_id.data,
               riff_chunk_id.count, riff_chunk_id.data,
               riff_chunk->header.size,
               riff_format.count, riff_format.data);

        for (Chunk_Node* n = first->next; n != 0; n = n->next)
        {
            Chunk_Header* header = n->header;

            if (header->id == FOURCC(FORMAT_CHUNK_ID))
            {
                Fmt_Chunk* format_chunk = (Fmt_Chunk*)header;

                String fmt_id = STR_FROM_FOURCC(format_chunk->header.id);
                String audio_format_str = wav_string_from_format(format_chunk->audio_format);

                printf("  Sub-Chunk %.*s {\n"
                       "    id            = %.*s,\n" 
                       "    size          = %u,\n"
                       "    audio format  = %.*s,\n" 
                       "    num channels  = %u,\n"
                       "    sample rate   = %u,\n"
                       "    byte rate     = %u,\n"
                       "    block align   = %u,\n"
                       "    bits per samp = %u\n"
                       "  },\n",
                       fmt_id.count, fmt_id.data,
                       fmt_id.count, fmt_id.data,
                       format_chunk->header.size,
                       audio_format_str.count, audio_format_str.data,
                       format_chunk->num_channels,
                       format_chunk->sample_rate,
                       format_chunk->byte_rate,
                       format_chunk->block_align,
                       format_chunk->bits_per_sample);
            }
            else if (header->id == FOURCC(DATA_CHUNK_ID))
            {
                Data_Chunk* data_chunk = (Data_Chunk*)header; 
                String data_id = STR_FROM_FOURCC(data_chunk->header.id);

                printf("  Sub-Chunk %.*s {\n"
                       "    id    = %.*s,\n" 
                       "    size  = %u,\n"
                       "  },\n",
                       data_id.count, data_id.data,
                       data_id.count, data_id.data,
                       data_chunk->header.size);
            }
            else
            {
                String unknown_chunk_id = STR_FROM_FOURCC(header->id);
                printf("  Sub-Chunk %.*s {\n"
                       "    id   = %.*s,\n" 
                       "    size = %u\n"
                       "  },\n",
                       unknown_chunk_id.count, unknown_chunk_id.data,
                       unknown_chunk_id.count, unknown_chunk_id.data,
                       header->size);
            }
        }
        printf("}\n"); // end of RIFF chunk
    }
    return 0;
}
