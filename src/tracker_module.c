#include "core.h"
#include "wav.h"
#include "wav_dump_utils.h"
#include "os_win32.h"

#include "fx.h"
#include "tracker.h"

#include "core.c"
#include "wav.c"
#include "wav_dump_utils.c"

#define OS_WIN32_DLL
#include "os_win32.c"

#include "fx.c"
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

    trk->pattern.bpm        = 180;
    trk->pattern.volume     = 0.9f;
    trk->pattern.cell_count = 16;
    trk->pattern.loop       = true;


    u32 delay_size = (u32)((60.0f / trk->pattern.bpm) * (f32)SUPPORTED_SAMPLE_RATE);
    printf("DELAY SIZE: %u\n", delay_size);

    // TODO[nr] @leak
    trk->feedback_comb_l = fx_feedback_comb_init(&trk->perm_arena, (u32)(delay_size/1.5f), 0.5f, 0.6f);
    trk->feedback_comb_r = fx_feedback_comb_init(&trk->perm_arena, (u32)(delay_size/1.5f), 0.5f, 0.6f);

#define TRK_ROW_EDIT(pattern, row_index)   for (Trk_Row* row = &(pattern)->rows[row_index]; row != 0; row = 0)
#define TRK_ROW_NEW(trk, name, fname, seq) u32 name = trk_pattern_add_row(&(trk)->perm_arena, &(trk)->asset_slop, &(trk)->pattern, fname, seq); TRK_ROW_EDIT(&(trk)->pattern, name)

#define TRK_CELL_EDIT(row, cell_index)                   for (Trk_Cell* cell = &(row)->cells[cell_index]; cell != 0; cell = 0)
#define TRK_CELL_RETRIG(cell, division, velocity, count) (cell)->retrig = (Trk_Retrig){division, velocity, count}

    TRK_ROW_NEW(trk, v2, STR_LIT("../data/vocals1.wav"),  STR_LIT("0001 0010 1000 1010"))
    {
        row->params.volume     = 0.1f;
        row->params.trim_left  = 0.25f;
        row->params.trim_right = 0.67f;
        row->params.delay      = 1.0f;
    }

    TRK_ROW_NEW(trk, kk, STR_LIT("../data/kick.wav"),  STR_LIT("1000 1000 1000 1000"))
    {
        row->params.volume = 0.2f;
    }

    TRK_ROW_NEW(trk, ch, STR_LIT("../data/ch.wav"),    STR_LIT("1010 1010 1010 1010"))
    {
        row->params.volume = 0.8f;
        row->params.pan    = 0.2f;

#if 1 
        TRK_CELL_EDIT(row, 0)  { cell->params.delay = 1.0f; }
        TRK_CELL_EDIT(row, 2)  { cell->params.delay = 1.0f; }
        TRK_CELL_EDIT(row, 4)  { cell->params.delay = 1.0f; }
        TRK_CELL_EDIT(row, 6)  { cell->params.delay = 1.0f; }
#endif
    }

    TRK_ROW_NEW(trk, oh, STR_LIT("../data/oh.wav"),    STR_LIT("0100 1000 0010 0000"))
    {
        row->params.volume = 0.1f;
        row->params.pitch = 0.1f;
    }

    TRK_ROW_NEW(trk, ss, STR_LIT("../data/snare.wav"), STR_LIT("0000 0010 0000 0010"))
    {
        row->params.volume = 0.2f;
        row->params.pan    = 0.0f;
        row->params.pitch  = 0.9f;

        TRK_CELL_EDIT(row, 6)  { cell->params.delay = 1.0f; }
        TRK_CELL_EDIT(row, 14) { cell->params.delay = 1.0f; }
    }

    // recompute the beat playhead in case the BPM changed
    trk->beat_playhead = trk->sample_playhead / trk_samples_per_beat(trk->pattern.bpm, (f32)SUPPORTED_SAMPLE_RATE);

    trk_pattern_print(&trk->pattern);

    return;
}

void trk_module_update(Trk* trk, Ring_Buffer* out)
{
    trk_play_pattern(trk, &trk->pattern, out);
    arena_reset(&trk->tran_arena);

    return;
}
