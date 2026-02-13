#ifndef WAV_H
#define WAV_H

#define FOURCC(s) *((u32*)(s))

#define STR_FROM_FOURCC(c)\
    STR_C(((char[5])\
    {\
        (char)(((c) >>  0) & 0xFF),\
        (char)(((c) >>  8) & 0xFF),\
        (char)(((c) >> 16) & 0xFF),\
        (char)(((c) >> 24) & 0xFF),\
        '\0'\
    }))

#define RIFF_CHUNK_ID         "RIFF"
#define RIFF_CHUNK_FORMAT_WAV "WAVE"
#define FORMAT_CHUNK_ID       "fmt "
#define DATA_CHUNK_ID         "data"

typedef struct
{
    u32 id;
    u32 size;
} Chunk_Header;

typedef struct Chunk_Node 
{
    Chunk_Header* header;
    u8*           data;

    struct Chunk_Node* next;
    struct Chunk_Node* last;
} Chunk_Node;

typedef struct
{
    Chunk_Header header;
    u32          format;
} RIFF_Chunk;

typedef struct Chunk_Node_V2
{
    Chunk_Header* header;
    u8*           data;

    struct Chunk_Node_V2* parent;
    struct Chunk_Node_V2* first_child;
    struct Chunk_Node_V2* next_sibling;
    struct Chunk_Node_V2* last_sibling;
} Chunk_Node_V2;

#define WAV_FORMAT_XLIST(X)\
    X(PCM,        0x0001)\
    X(IEE_FLOAT,  0x0003)\
    X(ALAW,       0x0006)\
    X(MULAW,      0x0007)\
    X(EXTENSIBLE, 0xFFFE)

typedef u16 Wav_Format;
enum
{
#define X(N, C) Wav_Format_##N = C,
    WAV_FORMAT_XLIST(X)
#undef X
};

typedef struct
{
    Chunk_Header header;
    Wav_Format   audio_format;
    u16          num_channels;
    u32          sample_rate;
    u32          byte_rate;   // bytes per second - (samples_per_second * bytes_per_sample * num_channels)
    u16          block_align; // bytes per frame  - (bytes_per_sample * num_channels)
    u16          bits_per_sample;
} Fmt_Chunk;

typedef struct
{
    Chunk_Header header;
    u8           data[];
} Data_Chunk;

// chunks
Chunk_Node* wav_chunks_from_file (Arena* arena, String file);
Fmt_Chunk*  wav_fmt_from_chunks  (Chunk_Node* first);
Data_Chunk* wav_data_from_chunks (Chunk_Node* first);
String      wav_file_from_chunks (Arena* arena, Chunk_Node* first);

Chunk_Node_V2* wav_chunks_from_file_v2 (Arena* arena, String file);
Chunk_Node_V2* wav_chunk_from_id       (Chunk_Node_V2* root, u32 id);
String         wav_file_from_chunks_v2 (Arena* arena, Chunk_Node_V2* root);

// strings
String wav_string_from_format(Wav_Format format);

#endif // WAV_H

