#ifndef WAV_H
#define WAV_H

#define WAV_FOURCC(s) *((u32*)(s))

#define WAV_FOURCC_FMT "%c%c%c%c"
#define WAV_FOURCC_ARG(c) \
    (char)(((c) >>  0) & 0xFF),\
    (char)(((c) >>  8) & 0xFF),\
    (char)(((c) >> 16) & 0xFF),\
    (char)(((c) >> 24) & 0xFF)

#define WAV_STR_FROM_FOURCC(c)\
    STR_C(((char[5])\
    {\
        (char)(((c) >>  0) & 0xFF),\
        (char)(((c) >>  8) & 0xFF),\
        (char)(((c) >> 16) & 0xFF),\
        (char)(((c) >> 24) & 0xFF),\
        '\0'\
    }))

#define WAV_RIFF_CHUNK_ID     "RIFF"
#define WAV_RIFF_CHUNK_FORMAT "WAVE"
#define WAV_FORMAT_CHUNK_ID   "fmt "
#define WAV_DATA_CHUNK_ID     "data"

#define WAV_FORMAT_SIZE 16
#define WAV_FORMAT_EXT_SIZE WAV_FORMAT_SIZE + 24


#define WAV_MIN_FILE_SIZE_FROM_DATA_SIZE(data_size) (sizeof(Wav_RIFF_Chunk) + sizeof(Wav_Chunk_Header) + WAV_FORMAT_SIZE + sizeof(Wav_Chunk_Header) + (data_size))

typedef struct
{
    u32 id;
    u32 size;
} Wav_Chunk_Header;

typedef struct
{
    Wav_Chunk_Header header;
    u32              format;
} Wav_RIFF_Chunk;

typedef struct Wav_Sub_Chunk_Node
{
    struct Wav_Sub_Chunk_Node* next;

    Wav_Chunk_Header header;
    u32              data_offset;
} Wav_Sub_Chunk_Node;

typedef struct
{
    Wav_Sub_Chunk_Node* first;
    Wav_Sub_Chunk_Node* last;

    u32 count;
} Wav_Sub_Chunk_List;

typedef struct
{
    Wav_RIFF_Chunk     riff_chunk;
    Wav_Sub_Chunk_List sub_chunks;
} Wav;

typedef struct Wav_Node
{
    struct Wav_Node* next;

    Wav_RIFF_Chunk     riff_chunk;
    Wav_Sub_Chunk_List sub_chunks;
} Wav_Node;

typedef struct
{
    Wav_Node* first;
    Wav_Node* last;

    u32 count;
} Wav_List;

#define WAV_FORMAT_TAG_XLIST(X)\
    X(PCM,        0x0001)\
    X(IEE_FLOAT,  0x0003)\
    X(ALAW,       0x0006)\
    X(MULAW,      0x0007)\
    X(EXTENSIBLE, 0xFFFE)

typedef u16 Wav_Format_Tag;
enum
{
#define X(N, C) Wav_Format_Tag_##N = C,
    WAV_FORMAT_TAG_XLIST(X)
#undef X
};

typedef struct
{
    Wav_Format_Tag format_tag;
    u16            num_channels;
    u32            sample_rate;
    u32            byte_rate;   // bytes per second -> (samples_per_second * bytes_per_sample * num_channels)
    u16            block_align; // bytes per frame  -> (bytes_per_sample * num_channels)
    u16            bits_per_sample;

    // Extended Format (when format_tag == EXTENSIBLE && cb_size == 22)
    u16 cb_size;
    u16 valid_bits_per_sample;
    u32 channel_mask;
    u8  sub_format[16];
} Wav_Format;

typedef struct
{
    u32 size;
    u8* buffer;
} Wav_Data;

typedef struct
{
    u16 num_channels;
    u16 bytes_per_sample;
    u32 sample_rate;
} Wav_Render_Params;

// parse
Wav      wav_from_data      (Arena* arena, String data);
Wav_List wav_list_from_data (Arena* arena, String data);

Wav_Sub_Chunk_List wav_sub_chunk_list_from_data (Arena* arena, String data);
Wav_Sub_Chunk_Node wav_sub_chunk_from_id        (Wav_Sub_Chunk_List* list, u32 id);

Wav_Format wav_get_format(Wav_Sub_Chunk_List* list, String data);
Wav_Data   wav_get_data  (Arena* arena, Wav_Sub_Chunk_List* list, String data);

// render
Wav_Data wav_render                    (Arena* arena, Wav_Render_Params params, Wav_Data data);
bool     wav_render_entire_file        (Arena* arena, Wav_Render_Params params, Wav_Data data, String filename);
bool     wav_render_append_data_to_file(String filename, Wav_Data data);

// strings
String wav_string_from_format_tag(Wav_Format_Tag format_tag);

#endif // WAV_H
