// parse
Chunk_Node* wav_chunks_from_file(Arena* arena, String file)
{
    Chunk_Node* first = 0;

    u8* chunk_readhead = (u8*)file.data;
    u8* end_of_file    = (u8*)file.data + file.count; // one past last
        
    // riff chunk

    RIFF_Chunk* riff_chunk = (RIFF_Chunk*)chunk_readhead;

    if (riff_chunk->header.id == FOURCC(RIFF_CHUNK_ID) && riff_chunk->format == FOURCC(RIFF_CHUNK_FORMAT_WAV))
    {
        // create first node of chunk list - the RIFF chunk
        first         = ARENA_PUSH_STRUCT(arena, Chunk_Node);
        first->header = (Chunk_Header*)chunk_readhead;
        first->data   = chunk_readhead + sizeof(Chunk_Header);
        first->next   = 0;
        first->last   = first;

        chunk_readhead += sizeof(RIFF_Chunk);
        Chunk_Header* chunk_header = (Chunk_Header*)chunk_readhead;

        while (chunk_readhead < end_of_file)
        {
            Chunk_Node* node = ARENA_PUSH_STRUCT(arena, Chunk_Node);
            node->header     = (Chunk_Header*)chunk_readhead;
            node->data       = chunk_readhead + sizeof(Chunk_Header);

            // last node points to new node
            first->last->next = node;
            first->last->last = node;

            // new node points nowhere
            node->next = 0;
            node->last = node;

            // last node becomes new node
            first->last = node;

            chunk_readhead += (sizeof(Chunk_Header) + chunk_header->size);
            chunk_header = (Chunk_Header*)chunk_readhead;
        }
    }

    return first;
}

Fmt_Chunk* wav_get_fmt_chunk(Chunk_Node* first)
{
    Fmt_Chunk* fmt_chunk = 0;

    RIFF_Chunk* riff_chunk = (RIFF_Chunk*)first->header;

    if (riff_chunk->header.id == FOURCC(RIFF_CHUNK_ID) && riff_chunk->format == FOURCC(RIFF_CHUNK_FORMAT_WAV))
    {
        for (Chunk_Node* n = first->next; n != 0; n = n->next)
        {
            Chunk_Header* header = n->header;

            if (header->id == FOURCC(FORMAT_CHUNK_ID))
            {
                fmt_chunk = (Fmt_Chunk*)header;
            }
        }
    }

    return fmt_chunk;
}

Data_Chunk* wav_get_data_chunk(Chunk_Node* first)
{
    Data_Chunk* data_chunk = 0;

    RIFF_Chunk* riff_chunk = (RIFF_Chunk*)first->header;

    if (riff_chunk->header.id == FOURCC(RIFF_CHUNK_ID) && riff_chunk->format == FOURCC(RIFF_CHUNK_FORMAT_WAV))
    {
        for (Chunk_Node* n = first->next; n != 0; n = n->next)
        {
            Chunk_Header* header = n->header;

            if (header->id == FOURCC(DATA_CHUNK_ID))
            {
                data_chunk = (Data_Chunk*)header;
            }
        }
    }

    return data_chunk;
}

String wav_file_from_chunks(Arena* arena, Chunk_Node* first)
{
    String result = {0};

    RIFF_Chunk* riff_chunk = (RIFF_Chunk*)first->header;

    if (riff_chunk->header.id == FOURCC(RIFF_CHUNK_ID) && riff_chunk->format == FOURCC(RIFF_CHUNK_FORMAT_WAV))
    {
        u32 file_size     = (sizeof(Chunk_Header) + first->header->size);
        u8* file_contents = ARENA_PUSH_ARRAY(arena, u8, file_size);

        u8* dest       = file_contents;
        u8* src_header = (u8*)first->header;
        u8* src_data   = first->data;

        // copy header
        for (u32 i = 0; i < sizeof(Chunk_Header); i += 1)
        {
            *dest++ = *src_header++;
        }

        // copy data
        for (u32 i = 0; i < first->header->size; i += 1)
        {
            *dest++ = *src_data++;
        }

        result.data  = (char*)file_contents;
        result.count = file_size;
    }

    return result;
}

// parse
Chunk_Node_V2* wav_chunks_from_file_v2(Arena* arena, String file)
{
    Chunk_Node_V2* root = 0;

    u8* chunk_readhead = (u8*)file.data;
    u8* end_of_file    = (u8*)file.data + file.count; // one past last
        
    // riff chunk

    RIFF_Chunk* riff_chunk = (RIFF_Chunk*)chunk_readhead;

    if (riff_chunk->header.id == FOURCC(RIFF_CHUNK_ID) && riff_chunk->format == FOURCC(RIFF_CHUNK_FORMAT_WAV))
    {
        // create root of chunk tree - the RIFF chunk
        root         = ARENA_PUSH_STRUCT(arena, Chunk_Node_V2);
        root->header = (Chunk_Header*)chunk_readhead;
        root->data   = chunk_readhead + sizeof(Chunk_Header);

        root->first_child  = 0;
        root->next_sibling = 0;
        root->last_sibling = root;

        chunk_readhead += sizeof(RIFF_Chunk);
        Chunk_Header* chunk_header = (Chunk_Header*)chunk_readhead;

        while (chunk_readhead < end_of_file)
        {
            Chunk_Node_V2* node = ARENA_PUSH_STRUCT(arena, Chunk_Node_V2);
            node->header        = (Chunk_Header*)chunk_readhead;
            node->data          = chunk_readhead + sizeof(Chunk_Header);

            // first child
            if (root->first_child == 0)
            {
                root->first_child = node;

                // first child is the last and only sibling to itself
                root->first_child->last_sibling = node;
                root->first_child->next_sibling = 0;
            }
            else // sibling to first child
            {
                // last sibling node points to new node
                root->first_child->last_sibling->next_sibling = node;
                root->first_child->last_sibling->last_sibling = node;

                // new node points nowhere
                node->next_sibling = 0;
                node->last_sibling = node;

                // last node becomes new node
                root->first_child->last_sibling = node;
            }

            chunk_readhead += (sizeof(Chunk_Header) + chunk_header->size);
            chunk_header = (Chunk_Header*)chunk_readhead;
        }
    }

    return root;
}

Chunk_Node_V2* wav_chunk_from_id(Chunk_Node_V2* root, u32 id)
{
    Chunk_Node_V2* result = 0;

    RIFF_Chunk* riff_chunk = (RIFF_Chunk*)root->header;

    if (riff_chunk->header.id == FOURCC(RIFF_CHUNK_ID) && riff_chunk->format == FOURCC(RIFF_CHUNK_FORMAT_WAV))
    {
        for (Chunk_Node_V2* n = root->first_child; n != 0; n = n->next_sibling)
        {
            Chunk_Header* header = n->header;

            if (header->id == id)
            {
                result = n;
                break;
            }

            if (n->first_child)
            {
                wav_chunk_from_id(n->first_child, id);
            }
        }
    }

    return result;
}

String wav_file_from_chunks_v2(Arena* arena, Chunk_Node_V2* root)
{
    String result = {0};

    RIFF_Chunk* riff_chunk = (RIFF_Chunk*)root->header;

    if (riff_chunk->header.id == FOURCC(RIFF_CHUNK_ID) && riff_chunk->format == FOURCC(RIFF_CHUNK_FORMAT_WAV))
    {
        u32 file_size     = (sizeof(Chunk_Header) + root->header->size);
        u8* file_contents = ARENA_PUSH_ARRAY(arena, u8, file_size);

        u8* dest     = file_contents;
        u8* src      = (u8*)root->header;

        u32 bytes_to_write = 0;
        u32 bytes_written  = 0;

        // write RIFF Parent Chunk
        bytes_to_write = sizeof(RIFF_Chunk);
        for (u32 i = 0; i < bytes_to_write; i += 1)
        {
            *dest++ = *src++;
        }
        bytes_written += bytes_to_write;

        // write remaining sub-chunks
        for (Chunk_Node_V2* n = root->first_child; n != 0; n = n->next_sibling)
        {
            src = (u8*)n->header;

            bytes_to_write = sizeof(Chunk_Header) + n->header->size;
            for (u32 i = 0; i < bytes_to_write; i += 1)
            {
                *dest++ = *src++;
            }
            bytes_written += bytes_to_write;
        }

        assert(bytes_written == file_size);

        result.data  = (char*)file_contents;
        result.count = file_size;
    }

    return result;
}

String wav_string_from_format(Wav_Format format)
{
    switch (format)
    {
#define X(N, C) case C: { return STR_LIT(#N); } break;
        WAV_FORMAT_XLIST(X)
#undef X
        default:
        {
            return STR_LIT("ERROR");
        } break;
    }
}
