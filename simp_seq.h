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
    u32 next;
    u32 prev;

    String filename;

    Wav_Format format;
    Wav_Data   data;
} Sound_Asset_Slot;

typedef struct
{
    u32 first_slot;
    u32 last_slot;

    u32 first_free;
    u32 last_free;

    u32 first_unused;

    Sound_Asset_Slot slot[MAX_SOUND_ASSET_SLOTS];
    bool             used[MAX_SOUND_ASSET_SLOTS];
} Sound_Asset_Pool;

typedef struct
{
    u32 next;
    u32 prev;

    u32 asset_id;
    u32 playhead;

    f32 volume;
    f32 pan;
    f32 pitch;

    bool loop;
} Sound_Instance_Slot;

typedef struct
{
    u32 first_slot;
    u32 last_slot;

    u32 first_free;
    u32 last_free;

    u32 first_unused;

    Sound_Instance_Slot slot[MAX_SOUND_INSTANCE_SLOTS];
    bool                used[MAX_SOUND_INSTANCE_SLOTS];
} Sound_Instance_Pool;

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

    u32 playhead;

    Sequence_Row rows[MAX_SEQUENCE_ROWS];
} Sequence;

typedef struct
{
    bool initialized;

    u32 sound_asset_id;
    u32 sound_instance_id;
    s32 pan_direction;

    Sequence sequence;

    Arena perm_arena;
    Arena tran_arena;

    Sound_Asset_Pool    asset_pool;
    Sound_Instance_Pool instance_pool;

    u32 total_samples_mixed;
    float mix_buffer[9600]; // TODO[nr] @better: assuming 100ms 48KHz stereo 
} Simp_Seq_State;

void ss_update(Simp_Seq_State* ss, Ring_Buffer* out);

#endif // SIMP_SEQ_H
