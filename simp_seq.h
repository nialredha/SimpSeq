#ifndef SIMP_SEQ_H
#define SIMP_SEQ_H

#define SUPPORTED_FORMAT_TAG    (Wav_Format_Tag_IEE_FLOAT)
#define SUPPORTED_BIT_DEPTH     (32)
#define SUPPORTED_CHANNEL_COUNT (2)
#define SUPPORTED_SAMPLE_RATE   (48000)

#define MAX_SOUND_ASSET_SLOTS    (32)
#define MAX_SOUND_INSTANCE_SLOTS (64)

#define MAX_SEQUENCE_CELLS (32)
#define MAX_SEQUENCE_ROWS  (32)

typedef struct
{
    String filename;

    Wav_Format format;
    Wav_Data   data;
} Sound_Asset;

typedef struct
{
    Slop slop;

    Sound_Asset assets[SLOP_MAX_SLOTS];
} Sound_Asset_Slop;

typedef struct
{
    u32 asset_id;
    u32 playhead;

    f32 volume;
    f32 pan;
    f32 pitch;

    bool loop;
} Sound;

typedef struct
{
    Slop slop;

    Sound sounds[SLOP_MAX_SLOTS];
} Sound_Slop;

typedef struct
{
    bool active;

    f32  volume;
    f32  pitch;
    f32  pan;
} Sequence_Cell;

typedef struct
{
    u32 asset_id;

    f32 volume;
    f32 pitch;
    f32 pan;

    bool solo;

    Sequence_Cell cells[MAX_SEQUENCE_CELLS];
} Sequence_Row;

typedef struct
{
    f32 bpm;

    f32 volume;
    f32 pitch;
    f32 pan;

    u32 cell_count;
    u32 row_count;

    u32  playhead;
    bool loop;

    Sequence_Row rows[MAX_SEQUENCE_ROWS];
} Sequence;

typedef struct
{
    Sequence sequence;

    Sound_Asset_Slop asset_slop;
    Sound_Slop       sound_slop;

    Arena perm_arena;
    Arena tran_arena;

    u32 total_samples_mixed;
    f32 mix_buffer[9600]; // TODO[nr] @better: assuming 100ms 48KHz stereo 
} Simp_Seq_State;

void ss_post_load  (Simp_Seq_State* ss);
void ss_post_reload(Simp_Seq_State* ss);

void ss_update(Simp_Seq_State* ss, Ring_Buffer* out);

#endif // SIMP_SEQ_H
