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


        u32 bytes_per_sample = 0;
        u32 bytes_per_frame  = 0;

        for (Chunk_Node* n = first->next; n != 0; n = n->next)
        {
            Chunk_Header* header = n->header;

            if (header->id == FOURCC(FORMAT_CHUNK_ID))
            {
                Fmt_Chunk* fmt_chunk = (Fmt_Chunk*)header;

                String fmt_id = STR_FROM_FOURCC(fmt_chunk->header.id);
                String audio_fmt_str = wav_string_from_format(fmt_chunk->audio_format);

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
                       fmt_chunk->header.size,
                       audio_fmt_str.count, audio_fmt_str.data,
                       fmt_chunk->num_channels,
                       fmt_chunk->sample_rate,
                       fmt_chunk->byte_rate,
                       fmt_chunk->block_align,
                       fmt_chunk->bits_per_sample);

                bytes_per_sample = (fmt_chunk->bits_per_sample / 8);
                bytes_per_frame  = bytes_per_sample * fmt_chunk->num_channels;
            }
            else if (header->id == FOURCC(DATA_CHUNK_ID))
            {
                Data_Chunk* data_chunk = (Data_Chunk*)header; 

                String data_id = STR_FROM_FOURCC(data_chunk->header.id);

                printf("  Sub-Chunk %.*s {\n"
                       "    id    = %.*s,\n" 
                       "    size  = %u,\n"
                       "    samples {",
                       data_id.count, data_id.data,
                       data_id.count, data_id.data,
                       data_chunk->header.size);

                u32 total_bytes    = data_chunk->header.size;
                u32 bytes_per_line = 4 * bytes_per_frame;
                u32 line_count     = 1;

                for (u32 byte_index = 0, byte_count = 1; 
                     byte_index < total_bytes && bytes_per_frame != 0; 
                     byte_index += 1, byte_count += 1)
                {
                    // new line 
                    if (byte_index % bytes_per_line == 0)
                    {
                        printf("\n      %08X  ", line_count * bytes_per_line); // byte offset
                        line_count++;
                    }

                    // print byte in hex
                    printf("%02X ", data_chunk->data[byte_index]);

                    // indent to group each sample on a line
                    if (byte_count % bytes_per_sample == 0)
                    {
                        printf(" "); 
                    }

                    // indent to group each frame on a line
                    if (byte_count % bytes_per_frame == 0)
                    {
                        printf(" "); 
                    }
                }
                printf("\n");
                printf("    }\n");
                printf("  },\n");
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
