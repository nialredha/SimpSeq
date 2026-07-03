//
// parse
//

Wav_List wav_list_from_data(Arena* arena, String data)
{
    Wav_List list = {0};

    u8* data_readhead = (u8*)data.data;
    u8* data_end      = (u8*)data.data + data.count; // one past last

    Wav_RIFF_Chunk* riff_chunk = (Wav_RIFF_Chunk*)data_readhead;

    while (data_readhead < data_end)
    {
        if (riff_chunk->header.id == WAV_FOURCC(WAV_RIFF_CHUNK_ID) && riff_chunk->format == WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT))
        {
            Wav_Node* node    = ARENA_PUSH_STRUCT(arena, Wav_Node);
            String data_slice = str((char*)data_readhead, (u32)(data_end - data_readhead));

            node->riff_chunk = *riff_chunk;
            node->sub_chunks = wav_sub_chunk_list_from_data(arena, data_slice);
            node->next = 0;

            // first node
            if (list.last == 0)
            {
                list.first = node;
                list.last  = node;
            }
            else // next node
            {
                list.last->next = node;
                list.last = node;
            }

            list.count++;

            data_readhead += (sizeof(Wav_Chunk_Header) + node->riff_chunk.header.size);
        }
    }

    return list;
}

Wav wav_from_data(Arena* arena, String data)
{
    Wav wav = {0};

    u8* data_readhead = (u8*)data.data;
    u8* data_end      = (u8*)data.data + data.count; // one past last

    Wav_RIFF_Chunk* riff_chunk = (Wav_RIFF_Chunk*)data_readhead;

    if (riff_chunk->header.id == WAV_FOURCC(WAV_RIFF_CHUNK_ID) && riff_chunk->format == WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT))
    {
        String data_slice = str((char*)data_readhead, (u32)(data_end - data_readhead));

        wav.riff_chunk = *riff_chunk;
        wav.sub_chunks = wav_sub_chunk_list_from_data(arena, data_slice);
    }

    return wav;
}

Wav_Sub_Chunk_List wav_sub_chunk_list_from_data(Arena* arena, String data)
{
    Wav_Sub_Chunk_List list = {0};

    u8* data_start    = (u8*)data.data;
    u8* data_readhead = (u8*)data.data;
    u8* data_end      = (u8*)data.data + data.count; // one past last
        
    // check for riff chunk

    Wav_RIFF_Chunk* riff_chunk = (Wav_RIFF_Chunk*)data_readhead;

    if (riff_chunk->header.id == WAV_FOURCC(WAV_RIFF_CHUNK_ID) && riff_chunk->format == WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT))
    {
        data_readhead += sizeof(Wav_RIFF_Chunk);

        while (data_readhead < data_end)
        {
            Wav_Sub_Chunk_Node* node = ARENA_PUSH_STRUCT(arena, Wav_Sub_Chunk_Node);

            node->header      = *(Wav_Chunk_Header*)data_readhead;
            node->data_offset = (u32)((data_readhead + sizeof(Wav_Chunk_Header)) - data_start);
            node->next        = 0;

            // first node
            if (list.last == 0)
            {
                list.first = node;
                list.last  = node;
            }
            else // next node
            {
                list.last->next = node;
                list.last       = node;
            }

            list.count++;

            data_readhead += (sizeof(Wav_Chunk_Header) + node->header.size);
        }
    }

    return list;
}

Wav_Sub_Chunk_Node wav_sub_chunk_from_id(Wav_Sub_Chunk_List* list, u32 id)
{
    Wav_Sub_Chunk_Node result = {0};

    for (Wav_Sub_Chunk_Node* node = list->first; node != 0; node = node->next)
    {
        if (node->header.id == id)
        {
            result = *node;
            break;
        }
    }

    return result;
}

Wav_Format wav_get_format(Wav_Sub_Chunk_List* list, String data)
{
    Wav_Format result = {0};

    Wav_Sub_Chunk_Node fmt_node = wav_sub_chunk_from_id(list, WAV_FOURCC(WAV_FORMAT_CHUNK_ID));

    Wav_Format* fmt = (Wav_Format*)&data.data[fmt_node.data_offset];
    if (fmt_node.header.size == 16)
    {
        result.format_tag      = fmt->format_tag;
        result.num_channels    = fmt->num_channels;
        result.sample_rate     = fmt->sample_rate;
        result.byte_rate       = fmt->byte_rate;
        result.block_align     = fmt->block_align;
        result.bits_per_sample = fmt->bits_per_sample;
    }
    else if (fmt_node.header.size == 16 + 22)
    {
        result.format_tag      = fmt->format_tag;
        result.num_channels    = fmt->num_channels;
        result.sample_rate     = fmt->sample_rate;
        result.byte_rate       = fmt->byte_rate;
        result.block_align     = fmt->block_align;
        result.bits_per_sample = fmt->bits_per_sample;

        result.cb_size                = fmt->cb_size;
        result.valid_bits_per_sample  = fmt->valid_bits_per_sample;
        result.channel_mask           = fmt->channel_mask;
        for (u32 i = 0; i < sizeof(result.sub_format); ++i)
        {
            result.sub_format[i] = fmt->sub_format[i];
        }
    }
    else
    {
        // uh oh
        fprintf(stderr, "ERROR: Invalid format!\n");
    }
    
    return result;
}

Wav_Data wav_get_data(Arena* arena, Wav_Sub_Chunk_List* list, String data)
{
    Wav_Data result = {0};

    Wav_Sub_Chunk_Node data_node = wav_sub_chunk_from_id(list, WAV_FOURCC(WAV_DATA_CHUNK_ID));

    result.buffer = ARENA_PUSH_ARRAY(arena, u8, data_node.header.size);
    result.size   = data_node.header.size;

    u8* src  = (u8*)&data.data[data_node.data_offset];
    u8* dest = result.buffer;

    for (u32 i = 0; i < data_node.header.size; ++i)
    {
        *dest++ = *src++;
    }

    return result;
}

//
// render
//

bool wav_render_entire_file(Arena* arena, Wav_Render_Params params, Wav_Data data, String filename)
{
    Wav_Data render_data = wav_render(arena, params, data);

    String render_content = { .data = (char*)render_data.buffer, .count = render_data.size };

    bool result = os_write_entire_file(filename, render_content);

    return result;
}

bool wav_render_append_data_to_file(String filename, Wav_Data data)
{
    (void)data;
    (void)filename;

    bool result = false;

#if 0

    Arena_Temp temp_arena = arena_temp_begin(arena);
    {
        String file_data = os_read_from_file(filename, 0, WAV_MIN_FILE_SIZE_FROM_DATA_SIZE(0));
        Wav_Sub_Chunk_List sub_chunks = wav_sub_chunk_list_from_data(arena, file_data)

        // get RIFF chunk
        String file_data = os_read_from_file(filename, 0, sizeof(Wav_RIFF_Chunk));
        Wav_RIFF_Chunk* riff_chunk = (Wav_RIFF_Chunk*)file_data.data;

        assert(riff_chunk->header.id == WAV_FOURCC(WAV_RIFF_CHUNK_ID));
        assert(riff_chunk->format    == WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT));

        // update RIFF chunk header to reflect new size
        {
            riff_chunk->header.size += data.size;
            result = os_write_to_file(filename, file_data, 0);
        }

        // append new data
        if (result)
        {
            String contents = { .data = (char*)data.buffer, .count = data.size };
            result = os_append_to_file(filename, contents);
        }
    }
    arena_temp_end(temp_arena);
#endif

    return result;
}

Wav_Data wav_render(Arena* arena, Wav_Render_Params params, Wav_Data data)
{
    Wav_Data render = {0};
    {
        u32 render_size = WAV_MIN_FILE_SIZE_FROM_DATA_SIZE(data.size);

        render.buffer = ARENA_PUSH_ARRAY(arena, u8, render_size);
        render.size = render_size;

        u32 write_head = 0;

        // RIFF chunk
        {
            Wav_RIFF_Chunk riff_chunk = { 
                .header = { 
                    .id = WAV_FOURCC(WAV_RIFF_CHUNK_ID), 
                    .size = render_size - sizeof(Wav_Chunk_Header) 
                },
                .format = WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT)
            };

            // write riff chunk
            Wav_RIFF_Chunk* dest = (Wav_RIFF_Chunk*)render.buffer;
            *dest = riff_chunk;

            write_head += sizeof(Wav_RIFF_Chunk);
        }

        // fmt chunk
        {
            Wav_Chunk_Header format_header = { .id = WAV_FOURCC(WAV_FORMAT_CHUNK_ID), .size = WAV_FORMAT_SIZE };

            Wav_Format format = {
                .format_tag      = Wav_Format_Tag_IEE_FLOAT,
                .num_channels    = params.num_channels,
                .sample_rate     = params.sample_rate,
                .byte_rate       = params.sample_rate * params.bytes_per_sample * params.num_channels,
                .block_align     = params.bytes_per_sample * params.num_channels,
                .bits_per_sample = params.bytes_per_sample * 8
            };

            // write format header
            {
                Wav_Chunk_Header* dest = (Wav_Chunk_Header*)(render.buffer + write_head);
                *dest = format_header;

                write_head += sizeof(Wav_Chunk_Header);
            }


            // write format data
            {
                u8* src  = (u8*)&format;
                u8* dest = render.buffer + write_head;
                for (u32 i = 0; i < WAV_FORMAT_SIZE; ++i)
                {
                    *dest++ = *src++;
                }

                write_head += WAV_FORMAT_SIZE;
            }
        }

        // data chunk
        {
            // write data header
            {
                Wav_Chunk_Header data_header = { .id = WAV_FOURCC(WAV_DATA_CHUNK_ID), .size = data.size };

                Wav_Chunk_Header* dest = (Wav_Chunk_Header*)(render.buffer + write_head);
                *dest = data_header;

                write_head += sizeof(Wav_Chunk_Header);
            }

            // write data
            if (data.buffer != 0 && data.size > 0)
            {
                u8* src  = data.buffer;
                u8* dest = render.buffer + write_head;
                for (u32 i = 0; i < data.size; ++i)
                {
                    *dest++ = *src++;
                }

                write_head += data.size;
            }
        }

        assert(render.size == write_head);
    }

    return render;
}

//
// strings
//

String wav_string_from_format_tag(Wav_Format_Tag format_tag)
{
    switch (format_tag)
    {
#define X(N, C) case C: { return STR_LIT(#N); } break;
        WAV_FORMAT_TAG_XLIST(X)
#undef X
        default:
        {
            return STR_LIT("ERROR");
        } break;
    }
}
