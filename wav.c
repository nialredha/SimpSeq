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
