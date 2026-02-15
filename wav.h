#ifndef WAV_H
#define WAV_H

#define WAV_FOURCC(s) *((u32*)(s))

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

typedef struct Wav_Chunk_Node
{
    Wav_Chunk_Header* header;
    u8*               data;

    struct Wav_Chunk_Node* parent;
    struct Wav_Chunk_Node* first_child;
    struct Wav_Chunk_Node* next_sibling;
    struct Wav_Chunk_Node* last_sibling;
} Wav_Chunk_Node;

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
    u32            byte_rate;   // bytes per second - (samples_per_second * bytes_per_sample * num_channels)
    u16            block_align; // bytes per frame  - (bytes_per_sample * num_channels)
    u16            bits_per_sample;
} Wav_Format;

typedef struct
{
    Wav_Format format;
    
    u16 cb_size;
    u16 valid_bits_per_sample;
    u32 channel_mask;
    u8  sub_format[16];
} Wav_Format_Ext;

typedef struct
{
    u32 size;
    u8* buffer;
} Wav_Data;

// chunks
Wav_Chunk_Node* wav_chunks_from_file (Arena* arena, String file);
Wav_Chunk_Node* wav_chunk_from_id    (Wav_Chunk_Node* root, u32 id);
String          wav_file_from_chunks (Arena* arena, Wav_Chunk_Node* root);

// strings
String wav_string_from_format_tag(Wav_Format_Tag format_tag);

#endif // WAV_H
