// parse
Wav_Chunk_Node* wav_chunks_from_file(Arena* arena, String file)
{
    Wav_Chunk_Node* root = 0;

    u8* chunk_readhead = (u8*)file.data;
    u8* end_of_file    = (u8*)file.data + file.count; // one past last
        
    // riff chunk

    Wav_RIFF_Chunk* riff_chunk = (Wav_RIFF_Chunk*)chunk_readhead;

    if (riff_chunk->header.id == WAV_FOURCC(WAV_RIFF_CHUNK_ID) && riff_chunk->format == WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT))
    {
        // create root of chunk tree - the RIFF chunk
        root         = ARENA_PUSH_STRUCT(arena, Wav_Chunk_Node);
        root->header = (Wav_Chunk_Header*)chunk_readhead;
        root->data   = chunk_readhead + sizeof(Wav_Chunk_Header);

        root->first_child  = 0;
        root->next_sibling = 0;
        root->last_sibling = root;

        chunk_readhead += sizeof(Wav_RIFF_Chunk);
        Wav_Chunk_Header* chunk_header = (Wav_Chunk_Header*)chunk_readhead;

        while (chunk_readhead < end_of_file)
        {
            Wav_Chunk_Node* node = ARENA_PUSH_STRUCT(arena, Wav_Chunk_Node);
            node->header        = (Wav_Chunk_Header*)chunk_readhead;
            node->data          = chunk_readhead + sizeof(Wav_Chunk_Header);

            // first child
            if (root->first_child == 0)
            {
                root->first_child = node;

                // first child is the last and only sibling to itself
                root->first_child->first_child  = 0;
                root->first_child->next_sibling = 0;
                root->first_child->last_sibling = node;
            }
            else // sibling to first child
            {
                // last sibling node points to new node
                root->first_child->last_sibling->next_sibling = node;
                root->first_child->last_sibling->last_sibling = node;

                // new node points nowhere
                node->first_child  = 0;
                node->next_sibling = 0;
                node->last_sibling = node;

                // last node becomes new node
                root->first_child->last_sibling = node;
            }

            chunk_readhead += (sizeof(Wav_Chunk_Header) + chunk_header->size);
            chunk_header = (Wav_Chunk_Header*)chunk_readhead;
        }
    }

    return root;
}

Wav_Chunk_Node* wav_chunk_from_id(Wav_Chunk_Node* root, u32 id)
{
    Wav_Chunk_Node* result = 0;

    Wav_RIFF_Chunk* riff_chunk = (Wav_RIFF_Chunk*)root->header;

    if (riff_chunk->header.id == WAV_FOURCC(WAV_RIFF_CHUNK_ID) && riff_chunk->format == WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT))
    {
        for (Wav_Chunk_Node* n = root->first_child; n != 0; n = n->next_sibling)
        {
            Wav_Chunk_Header* header = n->header;

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

String wav_file_from_chunks(Arena* arena, Wav_Chunk_Node* root)
{
    String result = {0};

    Wav_RIFF_Chunk* riff_chunk = (Wav_RIFF_Chunk*)root->header;

    if (riff_chunk->header.id == WAV_FOURCC(WAV_RIFF_CHUNK_ID) && riff_chunk->format == WAV_FOURCC(WAV_RIFF_CHUNK_FORMAT))
    {
        u32 file_size     = (sizeof(Wav_Chunk_Header) + root->header->size);
        u8* file_contents = ARENA_PUSH_ARRAY(arena, u8, file_size);

        u8* dest     = file_contents;
        u8* src      = (u8*)root->header;

        u32 bytes_to_write = 0;
        u32 bytes_written  = 0;

        // write RIFF Parent Chunk
        bytes_to_write = sizeof(Wav_RIFF_Chunk);
        for (u32 i = 0; i < bytes_to_write; i += 1)
        {
            *dest++ = *src++;
        }
        bytes_written += bytes_to_write;

        // write remaining sub-chunks
        for (Wav_Chunk_Node* n = root->first_child; n != 0; n = n->next_sibling)
        {
            src = (u8*)n->header;

            bytes_to_write = sizeof(Wav_Chunk_Header) + n->header->size;
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
