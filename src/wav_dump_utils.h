#ifndef WAV_DUMP_UTILS
#define WAV_DUMP_UTILS

#define WAV_DB_D_PARAM_BPL  (16)    // bytes per line
#define WAV_DB_D_PARAM_BPG  (8)     // bytes per group
#define WAV_DB_D_PARAM_LEVL (0)     // indent level
#define WAV_DB_D_PARAM_DOFF (false) // disable offset
#define WAV_DB_D_PARAM_DASC (false) // disable ascii

#define WAV_DUMP_BIN(arena, data, size, strb)                          wav_dump_bin(arena, 0,            WAV_DB_D_PARAM_BPL, WAV_DB_D_PARAM_BPG, WAV_DB_D_PARAM_LEVL, WAV_DB_D_PARAM_DOFF, WAV_DB_D_PARAM_DASC, data, size, strb);
#define WAV_DUMP_BIN_EXT(arena, data, size, strb, start_offset, level) wav_dump_bin(arena, start_offset, WAV_DB_D_PARAM_BPL, WAV_DB_D_PARAM_BPG, level,               WAV_DB_D_PARAM_DOFF, WAV_DB_D_PARAM_DASC, data, size, strb);

void wav_dump_riff     (Arena* arena, Wav_RIFF_Chunk* riff_chunk, u32 level, String_Builder* out);
void wav_dump_sub_chunk(Arena* arena, Wav_Sub_Chunk_Node* sub_chunk, u32 level, String_Builder* out);
void wav_dump_format   (Arena* arena, Wav_Format* format, u32 level, String_Builder* out);

void wav_dump_bin(Arena* arena, u32 start_offset, u32 bytes_per_line, u32 bytes_per_group, u32 level, bool disable_offset, bool disable_ascii, u8* data, u32 size, String_Builder* out);

void wav_dump_progress_bar(Arena* arena, u32 bytes_processed, u32 total_bytes, u32 width, String_Builder* out);

#endif // WAV_DUMP_UTILS

