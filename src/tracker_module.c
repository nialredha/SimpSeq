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

    trk->pattern.bpm        = 170;
    trk->pattern.volume     = 0.8f;
    trk->pattern.cell_count = 16;
    trk->pattern.loop       = true;

#define TRK_ROW_EDIT(pattern, row_index)   for (Trk_Row* row = &(pattern)->rows[row_index]; row != 0; row = 0)
#define TRK_ROW_NEW(trk, name, fname, seq) u32 name = trk_pattern_add_row(&(trk)->perm_arena, &(trk)->asset_slop, &(trk)->pattern, fname, seq); TRK_ROW_EDIT(&(trk)->pattern, name)

#define TRK_CELL_EDIT(row, cell_index)                   for (Trk_Cell* cell = &(row)->cells[cell_index]; cell != 0; cell = 0)
#define TRK_CELL_RETRIG(cell, division, velocity, count) (cell)->retrig = (Trk_Retrig){division, velocity, count}

    TRK_ROW_NEW(trk, kk, STR_LIT("../data/kick.wav"),  STR_LIT("1010 0000 0110 0001"))
    {
        row->volume = 0.3f;
    }

    TRK_ROW_NEW(trk, ch, STR_LIT("../data/ch.wav"),    STR_LIT("1111 1111 1111 1111"))
    {
        row->volume = 0.8f;
        row->pan    = 0.7f;
    }

    TRK_ROW_NEW(trk, oh, STR_LIT("../data/oh.wav"),    STR_LIT("1000 1000 0000 0000"))
    {
        row->volume = 0.4f;

        TRK_CELL_EDIT(row, 0)
        {
            TRK_CELL_RETRIG(cell, .division=16.0f, .velocity=-0.0625f, .count=32);

            cell->pitch = 0.25f;
        }
    }

    TRK_ROW_NEW(trk, ss, STR_LIT("../data/snare.wav"), STR_LIT("0010 1100 1100 1001"))
    {
        row->volume = 0.4f;
        row->pan    = 0.5f;
        row->pitch  = 0.9f;

        TRK_CELL_EDIT(row, 4) { TRK_CELL_RETRIG(cell, .division=4.0f,  .velocity=-0.25f,  .count=3);  }
        TRK_CELL_EDIT(row, 5) { TRK_CELL_RETRIG(cell, .division=16.0f, .velocity=0.0625f, .count=16); }
        TRK_CELL_EDIT(row, 9) { TRK_CELL_RETRIG(cell, .division=8.0f,  .velocity=0.125f,  .count=8);  }
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
