#include "core.h"
#include "wav.h"
#include "wav_dump_utils.h"
#include "os_win32.h"

#include "tracker.h"

#include "core.c"
#include "wav.c"
#include "wav_dump_utils.c"

#define OS_WIN32_DLL
#include "os_win32.c"

#include "tracker.c"

void trk_module_post_load(Trk* trk)
{
    // initialize memory
    trk_init(trk);
    return;
}

void trk_module_post_reload(Trk* trk)
{
    trk_pattern_reset(&trk->pattern);

    trk->pattern.bpm        = 90;
    trk->pattern.volume     = 0.8f;
    trk->pattern.cell_count = 16;
    trk->pattern.loop       = true;

    Trk_Row* kk = trk_pattern_add_row(&trk->perm_arena, &trk->asset_slop, &trk->pattern, STR_LIT("../data/kick.wav"),  STR_LIT("1010 0000 0110 0001"));
    Trk_Row* ch = trk_pattern_add_row(&trk->perm_arena, &trk->asset_slop, &trk->pattern, STR_LIT("../data/ch.wav"),    STR_LIT("0101 0111 0001 0110"));
    Trk_Row* oh = trk_pattern_add_row(&trk->perm_arena, &trk->asset_slop, &trk->pattern, STR_LIT("../data/oh.wav"),    STR_LIT("1000 1000 0000 0000"));
    Trk_Row* ss = trk_pattern_add_row(&trk->perm_arena, &trk->asset_slop, &trk->pattern, STR_LIT("../data/snare.wav"), STR_LIT("0000 1100 1100 1000"));

    kk->volume = 0.3f;

    ch->volume = 0.4f;
    ch->pan    = 0.7f;

    oh->volume = 0.4f;
    // oh->cells[0].retrig = (Trk_Retrig){.division = 16.0f, .velocity = -0.0625f, .count = 32 };
    // oh->cells[0].pitch = 0.25f;

    ss->volume = 0.4f;
    ss->pan    = 0.5f;
    ss->cells[4].retrig = (Trk_Retrig){.division = 4.0f,  .velocity = -0.25f,  .count = 3 };
    // ss->cells[5].retrig = (Trk_Retrig){.division = 16.0f, .velocity = 0.0625f, .count = 16 };
    ss->cells[9].retrig = (Trk_Retrig){.division = 8.0f,  .velocity = 0.125f,  .count = 8 };

    // recompute the beat playhead in case the BPM changed
    f32 beats_per_second   = trk->pattern.bpm / 60.0f;
    f32 samples_per_second = (f32)SUPPORTED_SAMPLE_RATE;
    u32 samples_per_beat   = (u32)(samples_per_second / beats_per_second);

    trk->beat_playhead = trk->sample_playhead / samples_per_beat;

    trk_pattern_print(&trk->pattern);

    return;
}

void trk_module_update(Trk* trk, Ring_Buffer* out)
{
    trk_play_pattern(trk, &trk->pattern, out);
    arena_reset(&trk->tran_arena);

    return;
}
