#ifndef TRACKER_H
#define TRACKER_H

#define SUPPORTED_FORMAT_TAG    (Wav_Format_Tag_IEE_FLOAT)
#define SUPPORTED_BIT_DEPTH     (32)
#define SUPPORTED_CHANNEL_COUNT (2)
#define SUPPORTED_SAMPLE_RATE   (48000)

#define TRK_MAX_ASSETS (32)
#define TRK_MAX_SOUNDS (64)

#define TRK_MAX_CELLS (32)
#define TRK_MAX_ROWS  (32)

typedef struct
{
    String     filename;
    Wav_Format format;
    Wav_Data   data;
} Trk_Asset;

typedef struct
{
    Slop      slop;
    Trk_Asset assets[SLOP_MAX_SLOTS];
} Trk_Asset_Slop;

typedef struct
{
    f32 volume;     // multiply
    f32 pan;        // add
    f32 pitch;      // multiply
    f32 trim_left;  // multiply
    f32 trim_right; // multiply
    f32 delay;      // multiply

    bool use_envelope;
    FX_Envelope envelope; // multiply
} Trk_Params;

typedef struct
{
    u32        asset_id;
    s32        playhead;
    Trk_Params params;
    bool       loop;
} Trk_Sound;

typedef struct
{
    Slop      slop;
    Trk_Sound sounds[SLOP_MAX_SLOTS];
} Trk_Sound_Slop;

typedef struct
{
    f32 division;
    f32 velocity;
    u32 count;
} Trk_Retrig;

typedef struct
{
    bool       active;
    Trk_Params params;
    Trk_Retrig retrig;
} Trk_Cell;

typedef struct
{
    u32        asset_id;
    Trk_Params params;
    Trk_Cell   cells[TRK_MAX_CELLS];

    bool solo;
    bool mute;
} Trk_Row;

typedef struct
{
    f32 bpm;
    f32 volume;
    f32 pitch;
    f32 pan;

    u32 cell_count;
    u32 row_count;

    bool loop;

    Trk_Row rows[TRK_MAX_ROWS];
} Trk_Pattern;

typedef struct
{
    Trk_Pattern pattern;

    u32  beat_playhead;
    u32  sample_playhead;

    Trk_Asset_Slop asset_slop;
    Trk_Sound_Slop sound_slop;

    FX_Feedback_Comb feedback_comb_l;
    FX_Feedback_Comb feedback_comb_r;

    Arena perm_arena;
    Arena reload_arena;
    Arena update_arena;
} Trk;

static u32        trk_asset_add         (Arena* arena, Trk_Asset_Slop* asset_slop, String filename);
static void       trk_asset_rem         (Trk_Asset_Slop* asset_slop, u32 slot_id);
static Trk_Asset* trk_asset_get         (Trk_Asset_Slop* asset_slop, u32 slot_id);
static Trk_Asset* trk_asset_find_or_load(Arena* arena, Trk_Asset_Slop* asset_slop, u32 slot_id);

static u32        trk_sound_add(Trk_Sound_Slop* sound_slop, u32 asset_id);
static void       trk_sound_rem(Trk_Sound_Slop* sound_slop, u32 slot_id);
static Trk_Sound* trk_sound_get(Trk_Sound_Slop* sound_slop, u32 slot_id);

static void trk_params_init(Trk_Params* params);

static void trk_row_from_str(Trk_Row* row, String str);

static void     trk_pattern_init   (Trk_Pattern* pattern, f32 bpm, u32 num_steps);
static void     trk_pattern_clear  (Trk_Pattern* pattern);
static u32 trk_pattern_add_row(Arena* arena, Trk_Asset_Slop* asset_slop, Trk_Pattern* pattern, String file, String sequence);
static void     trk_pattern_reset   (Trk_Pattern* pattern);
static void     trk_pattern_print   (Trk_Pattern* pattern);

static void trk_init        (Trk* trk);
static u32  trk_play_sound  (Trk* trk, u32 asset_id);
static void trk_stop_sound  (Trk* trk, u32 sound_id);
static void trk_play_pattern(Trk* state, Trk_Pattern* pattern, Ring_Buffer* out);
static u32  trk_mix         (Trk* trk, u32 samples_to_mix, Ring_Buffer* out);

static u32 trk_samples_per_beat(f32 bpm, f32 sample_rate);

#endif // TRACKER_H
