#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "base_io.h"
#include "wav.h"

#include "base_arena.c"
#include "base_string.c"
#include "base_io.c"
#include "wav.c"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("Not enough arguments!\n Example usage: .\\%s test.wav\n", argv[0]);
        return 1;
    }

    Arena arena = {0};
    arena_alloc(&arena, 4096);

    String filename     = STR_C(argv[1]);
    String file         = read_entire_file(filename);
    Wav_Chunk_Node* root = wav_chunks_from_file(&arena, file);

    Wav_RIFF_Chunk* riff_chunk = (Wav_RIFF_Chunk*)root->header;
    if (riff_chunk->header.id == WAV_FOURCC(WAV_RIFF_CHUNK_ID) && 
        riff_chunk->format    == WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT))
    {
        String riff_chunk_id = WAV_STR_FROM_FOURCC(riff_chunk->header.id);
        String riff_format   = WAV_STR_FROM_FOURCC(riff_chunk->format);

        printf("Chunk %.*s {\n"
               "  id     = %.*s,\n"
               "  size   = %u,\n"
               "  format = %.*s\n",
               riff_chunk_id.count, riff_chunk_id.data,
               riff_chunk_id.count, riff_chunk_id.data,
               riff_chunk->header.size,
               riff_format.count, riff_format.data);

        for (Wav_Chunk_Node* n = root->first_child; n != 0; n = n->next_sibling)
        {
            if (n->header->id == WAV_FOURCC(WAV_FORMAT_CHUNK_ID))
            {
                Wav_Format* format = (Wav_Format*)n->data;

                String fmt_id  = WAV_STR_FROM_FOURCC(n->header->id);
                String fmt_tag = wav_string_from_format_tag(format->format_tag);

                printf("  Sub-Chunk %.*s {\n"
                       "    id            = %.*s,\n" 
                       "    size          = %u,\n"
                       "    format tag    = %.*s,\n" 
                       "    num channels  = %u,\n"
                       "    sample rate   = %u,\n"
                       "    byte rate     = %u,\n"
                       "    block align   = %u,\n"
                       "    bits per samp = %u\n",
                       fmt_id.count, fmt_id.data,
                       fmt_id.count, fmt_id.data,
                       n->header->size,
                       fmt_tag.count, fmt_tag.data,
                       format->num_channels,
                       format->sample_rate,
                       format->byte_rate,
                       format->block_align,
                       format->bits_per_sample);

                if (format->format_tag == Wav_Format_Tag_EXTENSIBLE)
                {
                    Wav_Format_Ext* format_ext = (Wav_Format_Ext*)n->data;
                    printf("    cb size             = %d,\n"
                           "    valid bits per samp = %d,\n"
                           "    channel mask        = %08X,\n",
                           format_ext->cb_size,
                           format_ext->valid_bits_per_sample,
                           format_ext->channel_mask);

                    printf("    sub format {\n");
                    for (u32 i = 0; i < sizeof(format_ext->sub_format); i += 1)
                    {
                        printf("%02X ", format_ext->sub_format[i]);
                        if ((i + 1) % 8 == 0)
                        {
                            printf(" ");
                        }
                    }
                    printf("\n");
                    printf("    }\n");
                }
                printf("  },\n");
            }
            else if (n->header->id == WAV_FOURCC(WAV_DATA_CHUNK_ID))
            {
                String data_id = WAV_STR_FROM_FOURCC(n->header->id);

                printf("  Sub-Chunk %.*s {\n"
                       "    id    = %.*s,\n" 
                       "    size  = %u,\n"
                       "  },\n",
                       data_id.count, data_id.data,
                       data_id.count, data_id.data,
                       n->header->size);
            }
            else
            {
                String unknown_chunk_id = WAV_STR_FROM_FOURCC(n->header->id);
                printf("  Sub-Chunk %.*s {\n"
                       "    id   = %.*s,\n" 
                       "    size = %u\n"
                       "  },\n",
                       unknown_chunk_id.count, unknown_chunk_id.data,
                       unknown_chunk_id.count, unknown_chunk_id.data,
                       n->header->size);
            }
        }
        printf("}\n"); // end of RIFF chunk
    }

    return 0;
}
