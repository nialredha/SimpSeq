void wav_dump_riff(Arena* arena, Wav_RIFF_Chunk* riff_chunk, u32 level, String_Builder* out)
{
    u32 spaces_per_level = 2;

    u32 indent_0 = spaces_per_level * (level + 0);
    u32 indent_1 = spaces_per_level * (level + 1);

    strb_append(out, 
                str_format(arena, 
                           "%*sChunk " WAV_FOURCC_FMT " {\n"
                           "%*sid     = " WAV_FOURCC_FMT ",\n"
                           "%*ssize   = %u,\n"
                           "%*sformat = " WAV_FOURCC_FMT "\n"
                           "%*s}\n",
                           indent_0, "", WAV_FOURCC_ARG(riff_chunk->header.id),
                           indent_1, "", WAV_FOURCC_ARG(riff_chunk->header.id),
                           indent_1, "", riff_chunk->header.size,
                           indent_1, "", WAV_FOURCC_ARG(riff_chunk->format),
                           indent_0, ""));

    return;
}

void wav_dump_sub_chunk(Arena* arena, Wav_Sub_Chunk_Node* sub_chunk, u32 level, String_Builder* out)
{
    u32 spaces_per_level = 2;

    u32 indent_0 = spaces_per_level * (level + 0);
    u32 indent_1 = spaces_per_level * (level + 1);

    strb_append(out, 
                str_format(arena, 
                           "%*sSub-Chunk " WAV_FOURCC_FMT " {\n"
                           "%*sid     = " WAV_FOURCC_FMT ",\n"
                           "%*ssize   = %u,\n"
                           "%*s}\n",
                           indent_0, "", WAV_FOURCC_ARG(sub_chunk->header.id),
                           indent_1, "", WAV_FOURCC_ARG(sub_chunk->header.id),
                           indent_1, "", sub_chunk->header.size,
                           indent_0, ""));

    return;
}

void wav_dump_format(Arena* arena, Wav_Format* format, u32 level, String_Builder* out)
{
    u32 spaces_per_level = 2;
    u32 indent_0 = spaces_per_level * (level + 0);
    u32 indent_1 = spaces_per_level * (level + 1);

    String fmt_tag = wav_string_from_format_tag(format->format_tag);

    strb_append(out, 
                str_format(arena, 
                           "%*sformat tag    = %.*s,\n" 
                           "%*snum channels  = %u,\n"
                           "%*ssample rate   = %u,\n"
                           "%*sbyte rate     = %u,\n"
                           "%*sblock align   = %u,\n"
                           "%*sbits per samp = %u\n",
                           indent_1, "", fmt_tag.count, fmt_tag.data,
                           indent_1, "", format->num_channels,
                           indent_1, "", format->sample_rate,
                           indent_1, "", format->byte_rate,
                           indent_1, "", format->block_align,
                           indent_1, "", format->bits_per_sample));

    if (format->format_tag == Wav_Format_Tag_EXTENSIBLE)
    {
        strb_append(out, 
                    str_format(arena, 
                               "%*scb size             = %d,\n"
                               "%*svalid bits per samp = %d,\n"
                               "%*schannel mask        = %08X,\n"
                               "%*ssub format {\n",
                               indent_1, "", format->cb_size,
                               indent_1, "", format->valid_bits_per_sample,
                               indent_1, "", format->channel_mask,
                               indent_1, ""));

        WAV_DUMP_BIN_EXT(arena, format->sub_format, sizeof(format->sub_format), out, 0, level+2);
        strb_append(out, str_format(arena, "%*s}\n", indent_1, ""));
    }
    strb_append(out, str_format(arena, "%*s}\n", indent_0, ""));

    return;
}

void wav_dump_bin(Arena* arena, u32 start_offset, u32 bytes_per_line, u32 bytes_per_group, u32 level, bool disable_offset, bool disable_ascii, u8* data, u32 size, String_Builder* out)
{
    u32 spaces_per_level = 2;
    u32 indent_0 = spaces_per_level * (level + 0);

    u32 spaces_after_group = 2;
    u32 spaces_after_byte  = 1;

    u32 total_lines = 0;
    if (bytes_per_line > 0)
    {
        total_lines = (size + (bytes_per_line - 1)) / bytes_per_line; // ceil 
    }

    u32 total_bytes = size;

#if PROGRESS_BAR
    String_Builder progress_bar = {0};
    arena_alloc(&progress_bar.arena, 1024*1024);
#endif

    for (u32 line_index = 0; line_index < total_lines; line_index += 1)
    {
#if PROGRESS_BAR
        strb_clear(&progress_bar);
        wav_dump_progress_bar(arena, line_index, total_lines, 80, &progress_bar);
        printf("%.*s", progress_bar.str.count, progress_bar.str.data);
        fflush(stdout);
#endif

        Arena_Temp temp = arena_temp_begin(arena);

        if (!disable_offset)
        {
            // append byte offset of line
            u32 byte_offset = start_offset + (line_index * bytes_per_line);
            strb_append(out, str_format(temp.arena, "%*s%08X: ", indent_0, "", byte_offset));
        }

        u32 bytes_on_line = bytes_per_line < total_bytes ? bytes_per_line : total_bytes;

        u32 start_index = line_index * bytes_per_line;
        u32 stop_index  = start_index + bytes_on_line;

        // hex
        for (u32 byte_index = start_index; byte_index < stop_index; byte_index += 1)
        {
            // append spacing between bytes and groups
            if (byte_index > start_index && byte_index < stop_index)
            {
                if (byte_index % bytes_per_group == 0)
                {
                    // new group (excluding the first one)
                    strb_append(out, str_format(temp.arena, "%*s", spaces_after_group, ""));
                }
                else
                {
                    // new byte
                    strb_append(out, str_format(temp.arena, "%*s", spaces_after_byte, ""));
                }
            }

            // append byte in hex
            strb_append(out, str_format(temp.arena, "%02X", data[byte_index]));
        }

        if (!disable_ascii)
        {
            // ascii
            u32 byte_pad       = bytes_per_line - bytes_on_line;
            u32 chars_per_byte = 2 + spaces_after_byte;

            // append indent to align ascii
            u32 indent = (byte_pad * chars_per_byte) + (byte_pad / bytes_per_group);
            strb_append(out, str_format(temp.arena, "%*s |", indent, ""));

            for (u32 byte_index = start_index; byte_index < stop_index; byte_index += 1)
            {
                // print byte in ascii
                char c = (char)data[byte_index];
                if (c >= 32 && c <= 126)
                {
                    strb_append(out, str_format(temp.arena, "%c", c));
                }
                else
                {
                    strb_append(out, STR_LIT("."));
                }
            }
            strb_append(out, str_format(temp.arena, "%*s|", byte_pad));
        }

        // end of line
        strb_append(out, STR_LIT("\n"));

        total_bytes -= bytes_per_line;

        arena_temp_end(temp);
    }

#if PROGRESS_BAR
    printf("\n"); fflush(stdout);
    arena_free(&progress_bar.arena);
#endif
}

void wav_dump_progress_bar(Arena* arena, u32 bytes_processed, u32 total_bytes, u32 width, String_Builder* out)
{
    f32 frac_complete = (f32)bytes_processed / total_bytes;

    u32 fill_width = (u32)(frac_complete * width);

    strb_append(out, STR_LIT("\r["));
    for (u32 i = 0; i < width; i += 1)
    {
        if (i < fill_width)
        {
            strb_append(out, STR_LIT("#"));
        }
        else
        {
            strb_append(out, STR_LIT("."));
        }
    }

    Arena_Temp temp = arena_temp_begin(arena);
    strb_append(out, str_format(arena, "] %3d%% ", (u32)(frac_complete*100)));
    arena_temp_end(temp);
}
