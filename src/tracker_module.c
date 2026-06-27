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

    trk->pattern.bpm        = 120;
    trk->pattern.volume     = 0.9f;
    trk->pattern.cell_count = 16;
    trk->pattern.loop       = true;


    u32 delay_size = (u32)((60.0f / trk->pattern.bpm) * (f32)SUPPORTED_SAMPLE_RATE);
    printf("DELAY SIZE: %u\n", delay_size);

    // TODO[nr] @leak
    trk->feedback_comb_l = fx_feedback_comb_init(&trk->perm_arena, delay_size*2, 0.7f, 0.9f);
    trk->feedback_comb_r = fx_feedback_comb_init(&trk->perm_arena, delay_size, 0.7f, 0.8f);

#define TRK_ROW_EDIT(pattern, row_index)   for (Trk_Row* row = &(pattern)->rows[row_index]; row != 0; row = 0)
#define TRK_ROW_NEW(trk, name, fname, seq) u32 name = trk_pattern_add_row(&(trk)->perm_arena, &(trk)->asset_slop, &(trk)->pattern, fname, seq); TRK_ROW_EDIT(&(trk)->pattern, name)

#define TRK_CELL_EDIT(row, cell_index)                   for (Trk_Cell* cell = &(row)->cells[cell_index]; cell != 0; cell = 0)
#define TRK_CELL_RETRIG(cell, division, velocity, count) (cell)->retrig = (Trk_Retrig){division, velocity, count}

    // TODO[nr]: ability to do trimming at a cell level and delay at a row level
    TRK_ROW_NEW(trk, v1, STR_LIT("../data/vocals1.wav"),  STR_LIT("0010 0000 0000 0000"))
    {
        row->volume     = 0.3f;
        row->trim_left  = 0.1f;
        row->trim_right = 0.77f;
    }

    TRK_ROW_NEW(trk, v2, STR_LIT("../data/vocals1.wav"),  STR_LIT("0001 0010 1000 1010"))
    {
        row->volume     = 0.1f;
        row->trim_left  = 0.25f;
        row->trim_right = 0.67f;
    }

    TRK_ROW_NEW(trk, v3, STR_LIT("../data/vocals1.wav"),  STR_LIT("0000 0000 1001 0000"))
    {
        row->volume     = 0.3f;
        row->trim_left  = 0.34f;
        row->trim_right = 0.6f;

        TRK_CELL_EDIT(row, 8)
        {
            cell->delay = 1.0f;
        }

        TRK_CELL_EDIT(row, 11)
        {
            TRK_CELL_RETRIG(cell, .division=32.0f, .velocity=-0.025f, .count=32);
            cell->volume = 0.2f;
            cell->delay = 0.9f;
        }
    }

    TRK_ROW_NEW(trk, v4, STR_LIT("../data/vocals1.wav"),  STR_LIT("0000 0000 0000 1000"))
    {
        row->volume     = 0.3f;
        row->trim_left  = 0.46f;
        row->trim_right = 0.455f;

        TRK_CELL_EDIT(row, 12)
        {
            cell->delay = 0.5f;
        }
    }

    TRK_ROW_NEW(trk, v5, STR_LIT("../data/vocals1.wav"),  STR_LIT("0000 0000 0000 0001"))
    {
        row->volume     = 0.3f;
        row->trim_left  = 0.5f;
        row->trim_right = 0.32f;

        TRK_CELL_EDIT(row, 12)
        {
            cell->delay = 0.8f;
        }
    }

    TRK_ROW_NEW(trk, kk, STR_LIT("../data/kick.wav"),  STR_LIT("1010 0010 0010 0010"))
    {
        row->volume = 0.4f;
    }

    TRK_ROW_NEW(trk, ch, STR_LIT("../data/ch.wav"),    STR_LIT("1111 1111 1111 1111"))
    {
        row->volume = 0.8f;
        row->pan    = 0.2f;

#if 1 
        TRK_CELL_EDIT(row, 0)  { cell->delay = 1.0f; }
        TRK_CELL_EDIT(row, 4)  { cell->delay = 1.0f; }
        TRK_CELL_EDIT(row, 8)  { cell->delay = 1.0f; }
        TRK_CELL_EDIT(row, 12) { cell->delay = 1.0f; }
#endif
    }

    TRK_ROW_NEW(trk, oh, STR_LIT("../data/oh.wav"),    STR_LIT("0100 1000 0010 0000"))
    {
        row->volume = 0.4f;
        row->pitch = 0.95f;

        TRK_CELL_EDIT(row, 1)  { cell->delay = 0.8f; }
        TRK_CELL_EDIT(row, 10) { cell->delay = 0.8f; }
    }

    TRK_ROW_NEW(trk, ss, STR_LIT("../data/snare.wav"), STR_LIT("0000 0010 0000 0010"))
    {
        row->volume = 0.4f;
        row->pan    = 0.0f;
        row->pitch  = 0.9f;

        TRK_CELL_EDIT(row, 4) 
        { 
            // TRK_CELL_RETRIG(cell, .division=4.0f, .velocity=-0.25f, .count=3); 
            cell->delay = 0.6f;
        }
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
