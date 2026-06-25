#include "core.h"
#include "wav.h"
#include "wav_dump_utils.h"
#include "os_win32.h"

#include "core.c"
#include "wav.c"
#include "wav_dump_utils.c"

#define OS_WIN32_DLL
#include "os_win32.c"

#include "tracker.h"

static u32 trk_asset_add(Arena* arena, Trk_Asset_Slop* asset_slop, String filename)
{
    printf("\nAttempting to add asset %s\n", filename.data);

    // see if asset already exists and return its id if it does
    for (u32 slot_id = asset_slop->slop.first_slot; slot_id != 0; slot_id = asset_slop->slop.slots[slot_id].next)
    {
        Trk_Asset* asset = &asset_slop->assets[slot_id];

        if (str_compare(filename, asset->filename))
        {
            printf("  Already exists! ID: %u\n", slot_id);
            return slot_id;
        }
    }

    u32 slot_id = slop_slot_add(&asset_slop->slop);

    if (slot_id != 0)
    {
        Trk_Asset* asset = &asset_slop->assets[slot_id];

        asset->filename = str_copy(arena, filename);
        printf("  Succetrk! ID: %u\n", slot_id);
    }

    return slot_id;
}

static void trk_asset_rem(Trk_Asset_Slop* asset_slop, u32 slot_id)
{
    if (slop_slot_is_valid(&asset_slop->slop, slot_id))
    {
        // TODO[nr]: do we need to clear the data?
        // clear data
        Trk_Asset* asset = &asset_slop->assets[slot_id];
        *asset = (Trk_Asset){0};

        // remove from active slot pool
        slop_slot_rem(&asset_slop->slop, slot_id);
    }

    return;
}

static Trk_Asset* trk_asset_get(Trk_Asset_Slop* asset_slop, u32 slot_id)
{
    Trk_Asset* result = &asset_slop->assets[0];

    if (slop_slot_is_valid(&asset_slop->slop, slot_id))
    {
        result = &asset_slop->assets[slot_id];
    }

    return result;
}

static Trk_Asset* trk_asset_find_or_load(Arena* arena, Trk_Asset_Slop* asset_slop, u32 slot_id)
{
    Trk_Asset* result = &asset_slop->assets[0];

    if (slop_slot_is_valid(&asset_slop->slop, slot_id))
    {
        result = &asset_slop->assets[slot_id];

        if (result->data.buffer == 0)
        {
            // load the sound!
            String file = os_read_entire_file(result->filename);

            if (file.data != 0 && file.count > 0)
            {
                Wav wav = wav_from_data(arena, file);

                Wav_Format format = wav_get_format(&wav.sub_chunks, file);

                assert(format.format_tag      == SUPPORTED_FORMAT_TAG); 
                assert(format.bits_per_sample == SUPPORTED_BIT_DEPTH);
                assert(format.num_channels    == SUPPORTED_CHANNEL_COUNT);
                assert(format.sample_rate     == SUPPORTED_SAMPLE_RATE);

                Wav_Data data = wav_get_data(arena, &wav.sub_chunks, file);

                // printf("%s samples: %u\n", result->filename.data, data.size / 4);

                result->format = format;
                result->data   = data;

                os_free_file_contents(&file);
            }
        }
    }

    return result;
}

static u32 trk_sound_add(Trk_Sound_Slop* sound_slop, u32 asset_id)
{
    u32 slot_id = slop_slot_add(&sound_slop->slop);

    if (slot_id != 0)
    {
        Trk_Sound* sound = &sound_slop->sounds[slot_id];
        sound->asset_id = asset_id;
        sound->playhead = 0;
        sound->volume   = 0.5f;
        sound->pitch    = 1.0f;
        sound->pan      = 0.0f;
        sound->loop     = false;
    }

    return slot_id;
}

static void trk_sound_rem(Trk_Sound_Slop* sound_slop, u32 slot_id)
{
    if (slop_slot_is_valid(&sound_slop->slop, slot_id))
    {
        // clear data
        Trk_Sound* sound = &sound_slop->sounds[slot_id];
        *sound = (Trk_Sound){0};

        // remove from active slot pool
        slop_slot_rem(&sound_slop->slop, slot_id);
    }

    return;
}

static Trk_Sound* trk_sound_get(Trk_Sound_Slop* sound_slop, u32 slot_id)
{
    Trk_Sound* result = &sound_slop->sounds[0];

    if (slop_slot_is_valid(&sound_slop->slop, slot_id))
    {
        result = &sound_slop->sounds[slot_id];
    }

    return result;
}

static void trk_pattern_init(Trk_Pattern* pattern, f32 bpm, u32 num_steps)
{
    pattern->bpm = bpm;

    pattern->volume = 1.0f;
    pattern->pitch  = 1.0f;
    pattern->pan    = 0.0f;

    pattern->cell_count = num_steps;

    return;
}

static void trk_pattern_clear(Trk_Pattern* pattern)
{
    pattern->bpm = 0.0f;

    pattern->volume = 0.0;
    pattern->pitch  = 0.0;
    pattern->pan    = 0.0;

    pattern->cell_count = 0;
    pattern->row_count  = 0;

    return;
}

static Trk_Row* trk_pattern_add_row(Trk_Pattern* pattern)
{
    Trk_Row* result = 0;

    if (pattern->row_count < TRK_MAX_ROWS)
    {
        u32 row_index = pattern->row_count;
        result = &pattern->rows[row_index];

        result->volume = 1.0f;
        result->pitch  = 1.0f;
        result->pan    = 0.0f;

        result->trim_left = 0.0f;
        result->trim_right = 0.0f;

        result->solo = false;

        pattern->row_count += 1;
    }

    return result;
}

static Trk_Row* trk_pattern_set_row_from_str(Trk_Pattern* pattern, u32 row_id, String row_str)
{
    Trk_Row* row = 0;

    if (row_id < pattern->row_count)
    {
        row = &pattern->rows[row_id];

        u32 cell_index = 0; 
        while (*row_str.data != 0 && cell_index < pattern->cell_count)
        {
            str_eat_whitespace(&row_str);
            String cell_str = str_advance(&row_str, 1);

            switch (*cell_str.data)
            {
                case '0':
                {
                    row->cells[cell_index++] = (Trk_Cell){0};
                    break;
                }
                case '1':
                {
                    row->cells[cell_index++] = (Trk_Cell){true, 1.0, 1.0, 0.0};
                    break;
                }
                default:
                {
                    row = 0;
                    break;
                }
           }
        }
    }

    return row;
}

static Trk_Row* trk_pattern_add_row_from_str(Trk_Pattern* pattern, String row_str)
{
    Trk_Row* row = trk_pattern_add_row(pattern);

    if (row != 0)
    {
        row = trk_pattern_set_row_from_str(pattern, pattern->row_count - 1, row_str);
    }

    return row;
}

static Trk_Row* trk_pattern_add_row_from_int(Trk_Pattern* pattern, u32 row_int)
{
    Trk_Row* row = trk_pattern_add_row(pattern);

    if (row != 0)
    {
        u32 cell_index = 0;
        row_int <<= (TRK_MAX_CELLS - pattern->cell_count);

        while (row_int > 0 && cell_index < pattern->cell_count)
        {
            u32 cell_int = (row_int & (0x80000000)) >> (TRK_MAX_CELLS - 1);

            switch (cell_int)
            {
               case 0:
               {
                   row->cells[cell_index++] = (Trk_Cell){0};
                   break;
               }
               case 1:
               {
                   row->cells[cell_index++] = (Trk_Cell){true, 1.0, 1.0, 0.0};
                   break;
               }
               default:
               {
                   row = 0;
                   break;
               }
           }

           row_int <<= 1;
        }
    }

    return row;
}

static void trk_pattern_reset(Trk_Pattern* pattern)
{
    pattern->cell_count = 0;
    pattern->row_count = 0;

    return;
}

static void trk_pattern_print(Trk_Pattern* pattern)
{
    printf("\n");
    printf("   |");
    for (u32 cell_index = 0; cell_index < pattern->cell_count; ++cell_index)
    {
        printf("C%.2u|", cell_index);
    }
    printf("\n");

    for (u32 row_index = 0; row_index < pattern->row_count; ++row_index)
    {
        printf("R%.2u|", row_index);
        for (u32 cell_index = 0; cell_index < pattern->cell_count; ++cell_index)
        {
            if (pattern->rows[row_index].cells[cell_index].active)
            {
                printf(" 1 |");
            }
            else
            {
                printf(" 0 |");
            }
        }
        printf("\n");
    }

    printf("   |");
    for (u32 cell_index = 0; cell_index < pattern->cell_count; ++cell_index)
    {
        printf("---|");
    }
    printf("\n");
    printf("\n");
}

static void trk_init(Trk* trk)
{
    u8* dest = (u8*)trk;
    for (u32 byte_index = 0; byte_index < sizeof(*trk); byte_index++)
    {
        *dest++ = 0;
    }

    arena_alloc(&trk->perm_arena, 16*1024*1024);
    arena_alloc(&trk->tran_arena, 64*1024);

    return;
}

static u32 trk_play_sound(Trk* trk, u32 asset_id)
{
    u32 sound_id = 0;

    Trk_Asset* asset = trk_asset_find_or_load(&trk->perm_arena, &trk->asset_slop, asset_id);

    if (asset->data.buffer != 0)
    {
        sound_id = trk_sound_add(&trk->sound_slop, asset_id);
    }

    return sound_id;
}

static void trk_stop_sound(Trk* trk, u32 sound_id)
{
    trk_sound_rem(&trk->sound_slop, sound_id);

    return;
}

static void trk_play_pattern(Trk* trk, Trk_Pattern* pattern, Ring_Buffer* out)
{
    if (pattern->bpm > 0)
    {
        f32 beats_per_second   = pattern->bpm / 60.0f;
        f32 samples_per_second = (f32)SUPPORTED_SAMPLE_RATE;
        u32 samples_per_beat   = (u32)(samples_per_second / beats_per_second);

        u32 samples_to_mix = 9600; // TODO[nr] @better
        samples_to_mix = samples_to_mix > samples_per_beat ? samples_per_beat : samples_to_mix; // clamp to samples per beat

        u32 total_samples_in_pattern = samples_per_beat * pattern->cell_count;

        // check whether we need to loop the pattern
        if (pattern->loop && trk->sample_playhead >= total_samples_in_pattern)
        {
            trk->beat_playhead   = 0;
            trk->sample_playhead = 0;
        }

        if (trk->sample_playhead < total_samples_in_pattern)
        {
            f32 cell_pos   = (f32)trk->sample_playhead / (f32)samples_per_beat;
            u32 cell_index = (u32)cell_pos;

            bool solo_enabled = false;
            for (u32 row_index = 0; row_index < pattern->row_count; ++row_index)
            {
                if (pattern->rows[row_index].solo)
                {
                    solo_enabled = true;
                    break;
                }
            }

            if (cell_index >= trk->beat_playhead && cell_index < TRK_MAX_CELLS)
            {
                // beat boundary!

                // printf("cell_pos = %f, cell_index = %u, beat_playhead = %u, sample_playhead = %u, samples_per_beat = %u\n", cell_pos, cell_index, trk->beat_playhead, trk->sample_playhead, samples_per_beat);

                for (u32 row_index = 0; row_index < pattern->row_count; ++row_index)
                {
                    Trk_Row* row = &pattern->rows[row_index];
                    if (solo_enabled && !row->solo) { continue; }

                    Trk_Cell* cell = &row->cells[cell_index];

                    if (cell->active)
                    {
                        // retrig enabled
                        if (cell->retrig.count > 0 && cell->retrig.division > 1)
                        {
                            u32 samples_per_div = (u32)((f32)samples_per_beat / cell->retrig.division);

                            for (u32 div_index = 0; div_index < cell->retrig.count; ++div_index)
                            {
                                u32 sound_id = trk_play_sound(trk, row->asset_id);
                                Trk_Sound* sound = trk_sound_get(&trk->sound_slop, sound_id);

                                f32 a = 0.0f;
                                f32 b = cell->volume;
                                f32 t = cell->retrig.velocity * div_index;
                                if (cell->retrig.velocity < 0.0f)
                                {
                                    a = cell->volume;
                                    b = 0.0f;
                                    t *= -1.0f;
                                }

                                f32 cell_volume = lerp(a, b, t);

                                // printf("retrig a: %f, b: %f, t: %f, cell_volume %f\n", 0.0f, cell->volume, cell->retrig.velocity*div_index, cell_volume);

                                sound->volume = pattern->volume * row->volume * cell_volume;

                                f32 pan     = row->pan + cell->pan;
                                pan         = pan < -1.0f ? pan = -1.0f : pan > 1.0f ? pan = 1.0f : pan;
                                sound->pan  = pan;

                                sound->pitch  = row->pitch * cell->pitch;

                                sound->trim_left  = row->trim_left;
                                sound->trim_right = row->trim_right;   

                                sound->playhead = -1 * (div_index * samples_per_div);
                                // printf("div_index %u, count %u, samples_per_div %u, playhead %d\n\n", div_index, cell->retrig.count, samples_per_div, sound->playhead);
                            }
                            // printf("\n");

                            // printf("playing  sound! sound_id: %u, asset_id: %u, playhead: %u\n", sound_id, pattern->rows[row_index].asset_id, sound->playhead);
                        }
                        else
                        {
                            u32 sound_id = trk_play_sound(trk, row->asset_id);
                            Trk_Sound* sound = trk_sound_get(&trk->sound_slop, sound_id);

                            sound->volume = pattern->volume * row->volume * cell->volume;

                            f32 pan     = row->pan + cell->pan;
                            pan         = pan < -1.0f ? pan = -1.0f : pan > 1.0f ? pan = 1.0f : pan;
                            sound->pan  = pan;

                            sound->pitch  = row->pitch * cell->pitch;

                            sound->trim_left  = row->trim_left;
                            sound->trim_right = row->trim_right;
                        }

                        // printf("playing  sound! sound_id: %u, asset_id: %u, playhead: %u\n", sound_id, pattern->rows[row_index].asset_id, sound->playhead);
                    }
                }

                trk->beat_playhead++;
            }
            else
            {
                // between beats!
                u32 samples_til_next_beat = trk->sample_playhead % samples_per_beat;

                assert(samples_til_next_beat != 0);

                if (samples_til_next_beat < samples_to_mix)
                {
                    samples_to_mix = samples_til_next_beat;
                }
            }
        }

        f32* mix_buffer = ARENA_PUSH_ARRAY(&trk->tran_arena, f32, samples_to_mix);
        trk->sample_playhead += trk_mix(trk, mix_buffer, samples_to_mix, out);
    }
}

static u32 trk_mix(Trk* trk, f32* mix_buffer, u32 samples_to_mix, Ring_Buffer* out)
{
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // TODO[nr] @better: this whole thing depends on the WAV being f32 48KHz stereo!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    Trk_Asset_Slop* assets = &trk->asset_slop;
    Trk_Sound_Slop* sounds = &trk->sound_slop;

    u32 bytes_available   = (u32)out->amount_free;
    u32 bytes_per_sample  = sizeof(*mix_buffer);
    u32 samples_available = bytes_available / bytes_per_sample;

    samples_to_mix = samples_available < samples_to_mix ? samples_available : samples_to_mix;

    // clear buffer
    for (u32 mix_index = 0; mix_index < samples_to_mix; ++mix_index)
    {
        mix_buffer[mix_index] = 0;
    }

    for (u32 sound_id = sounds->slop.first_slot; sound_id != 0; )
    {
        Trk_Sound* sound = trk_sound_get(sounds, sound_id);
        Trk_Asset* asset = trk_asset_get(assets, sound->asset_id);

        u32 samples_in_asset  = asset->data.size / bytes_per_sample;

        u32 mix_index = 0;

        while (sound->playhead < 0 && mix_index < samples_to_mix)
        {
            sound->playhead++;
            mix_index++;
        }

        u32 byte_index  = sound->playhead * bytes_per_sample;
        f32* asset_data = (f32*)&asset->data.buffer[byte_index];

        u32 trim_left_samples     = (u32)((sound->trim_left  * samples_in_asset) + 0.5f);
        u32 trim_right_samples    = (u32)((sound->trim_right * samples_in_asset) + 0.5f);
        u32 total_samples_to_trim = trim_left_samples + trim_right_samples;

        if (total_samples_to_trim <= samples_in_asset)
        {
            samples_in_asset = samples_in_asset - total_samples_to_trim;
            byte_index       = (trim_left_samples + sound->playhead) * bytes_per_sample;
            asset_data       = (f32*)&asset->data.buffer[byte_index];
        }

        // TODO[nr] @study: currently doing equal power panning, but that makes center quieter...
#if 0
        f32 pan_0 = (1 - sound->pan);
        f32 pan_1 = (sound->pan);
#else
        f32 pan_0 = sound->pan < 0 ? 1.0f : 1 - sound->pan;
        f32 pan_1 = sound->pan > 0 ? 1.0f : 1 + sound->pan;
#endif

        u32 sample_index = 0;
        f32 d_sample = sound->pitch;

        f32 frames_remain = ((f32)(samples_in_asset - (sound->trim_left + sound->playhead))) / 2.0f;

        for (f32 frame_pos = 0; frame_pos < frames_remain; frame_pos += d_sample)
        {
            if (mix_index >= samples_to_mix) { break; }

            // u32 frame_index = (u32)(frame_pos + 0.5f);
            u32 frame_index = (u32)(frame_pos);

            f32 frac        = frame_pos - (f32)frame_index;
            sample_index    = frame_index * 2;

            f32 sample_0 = asset_data[sample_index + 0]; // 0L
            f32 sample_1 = asset_data[sample_index + 1]; // 0R

            if (sound->playhead + sample_index + 2 < samples_in_asset)
            {
                f32 sample_2 = asset_data[sample_index + 2]; // 1L
                f32 sample_3 = asset_data[sample_index + 3]; // 1R

                sample_0 = lerp(sample_0, sample_2, frac);
                sample_1 = lerp(sample_1, sample_3, frac);
            }               

            mix_buffer[mix_index]     += (sample_0 * sound->volume * pan_0);
            mix_buffer[mix_index + 1] += (sample_1 * sound->volume * pan_1);

            mix_index += 2;
        }

        sound->playhead += sample_index;

        u32 next_sound_id = sounds->slop.slots[sound_id].next;

        if (sound->playhead >= (s32)samples_in_asset - 2)
        {
            if (sound->loop)
            {
                sound->playhead = 0;
            }
            else
            {
                trk_sound_rem(sounds, sound_id);
            }
        }

        sound_id = next_sound_id;

    }

    u32 bytes_mixed = samples_to_mix * bytes_per_sample;

    u32 bytes_written = rb_write(out, (u8*)mix_buffer, bytes_mixed);

    assert(bytes_written == bytes_mixed);

    return samples_to_mix;
}

void trk_module_post_load(Trk* trk)
{
    // initialize memory
    trk_init(trk);
    return;
}

void trk_module_post_reload(Trk* trk)
{
    // recompute the beat playhead in case the BPM changed
    trk->beat_playhead = trk->sample_playhead / SUPPORTED_SAMPLE_RATE;

    trk_pattern_reset(&trk->pattern);

    // Row* row = add_row(STR_LIT("../data/vocals2.wav"), STR_LIT("1000 1000 0000 1000"));

    // ADD_ROW(STR_LIT("../data/vocals2.wav"),       STR_LIT("1000 1000 0000 1000"));
    // ADD_ROW(STR_LIT("../data/vocals5.wav"),       STR_LIT("0000 0000 0010 0000"));
    // ADD_ROW(STR_LIT("../data/ch.wav"),            STR_LIT("0101 0010 0101 0010"));
    // ADD_ROW(STR_LIT("../data/kick.wav"),          STR_LIT("1000 0000 1010 0010"));
    // ADD_ROW(STR_LIT("../data/ch.wav"),            STR_LIT("1111 1111 1111 1111"));
    // ADD_ROW(STR_LIT("../data/oh.wav"),            STR_LIT("0010 0010 0010 1001"));
    // ADD_ROW(STR_LIT("../data/oh.wav"),            STR_LIT("0001 1001 0001 0100"));
    // ADD_ROW(STR_LIT("../data/snare.wav"),         STR_LIT("0000 1000 0000 1000"));
    // ADD_ROW(STR_LIT("../data/snare.wav"),         STR_LIT("0000 0100 0000 0100"));
    // ADD_ROW(STR_LIT("../data/stage_grand_g.wav"), STR_LIT("1000 0000 0000 0010"));
    // ADD_ROW(STR_LIT("../data/stage_grand_g.wav"), STR_LIT("1000 0001 0000 0010"));

    // add assets
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/vocals2.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/vocals5.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/ch.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/kick.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/ch.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/oh.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/oh.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/snare.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/snare.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/stage_grand_g.wav"));
    trk_pattern_add_row(&trk->pattern)->asset_id = trk_asset_add(&trk->perm_arena, &trk->asset_slop, STR_LIT("../data/stage_grand_g.wav"));

    // trk->pattern.bpm = 30;
    // trk->pattern.bpm = 120;
    // trk->pattern.bpm = 103;
    trk->pattern.bpm = 175;
    // trk->pattern.bpm = 117;
    // trk->pattern.bpm = 187;
    // trk->pattern.bpm = 360;

    // trk->pattern.volume     = 0.8f;
    trk->pattern.volume     = 0.0f;
    trk->pattern.pitch      = 0.0f;
    trk->pattern.pan        = 0.0f;
    trk->pattern.cell_count = 16;

    trk->pattern.loop = true;

    String voice1_seq  = STR_LIT("1000 1000 0000 1000");
    String voice2_seq  = STR_LIT("0000 0000 0010 0000");
    String ch_0_seq    = STR_LIT("0101 0010 0101 0010");
    String kick_seq    = STR_LIT("1000 0000 1010 0010");
    String ch_seq      = STR_LIT("1111 1111 1111 1111");
    String oh_seq      = STR_LIT("0010 0010 0010 1001");
    String oh_seq_2    = STR_LIT("0001 1001 0001 0100");
    String snare_seq   = STR_LIT("0000 1000 0000 1000");
    String snare_seq_2 = STR_LIT("0000 0100 0000 0100");
    String batrk_seq    = STR_LIT("1000 0000 0000 0010");
    String batrk_seq_2  = STR_LIT("1000 0001 0000 0010");
    
    Trk_Row* row;

    row = trk_pattern_set_row_from_str(&trk->pattern, 0, voice1_seq);
    row->volume = 0.05f;
    row->pan    = -0.7f;
    row->trim_right = 0.8f;
    row->cells[12].retrig = (Trk_Retrig){.division = 2.0f, .velocity = -0.5f, .count = 2 };

    row = trk_pattern_set_row_from_str(&trk->pattern, 1, voice2_seq);
    row->volume = 0.1f;
    row->pan    = 0.7f;
    row->trim_right = 0.5f;
    row->pitch  = 0.5f;
    // row->cells[10].retrig = (Trk_Retrig){.division = 16.0f, .velocity = -0.125f/2.0f, .count = 8 };

    row = trk_pattern_set_row_from_str(&trk->pattern, 2, ch_0_seq);
    row->volume = 0.75f;
    row->pitch  = 0.5f;
    row->pan    = -0.7f;
    row->cells[9].retrig = (Trk_Retrig){.division = 4.0f, .velocity = -0.25f, .count = 4 };
    row->cells[9].pitch = 1.0f;

    for (u32 cell_index = 0; cell_index < trk->pattern.cell_count; ++cell_index)
    {
        if (cell_index % 2 == 0)
        {
            row->cells[cell_index].volume = 0.5f;
            row->cells[cell_index].pan = 0.9f;
        }
    }

    row = trk_pattern_set_row_from_str(&trk->pattern, 3, kick_seq);
    row->volume = 0.3f;

    row = trk_pattern_set_row_from_str(&trk->pattern, 4, ch_seq);
    row->volume = 0.4f;
    row->pan    = 0.7f;
    for (u32 cell_index = 0; cell_index < trk->pattern.cell_count; ++cell_index)
    {
        row->cells[cell_index].retrig = (Trk_Retrig){.division = 4.0f, .velocity = -0.25f, .count = 3 };
    }

    row = trk_pattern_set_row_from_str(&trk->pattern, 5, oh_seq);
    row->volume = 0.4f;

    row = trk_pattern_set_row_from_str(&trk->pattern, 6, oh_seq_2);
    row->volume = 0.2f;
    row->pan    = -0.5f;
    row->cells[11].retrig = (Trk_Retrig){.division = 2.0f, .velocity = -0.75f, .count = 2 };

    row = trk_pattern_set_row_from_str(&trk->pattern, 7, snare_seq);
    row->volume = 0.4f;
    row->pan    = 0.5f;
    row->cells[4].retrig = (Trk_Retrig){.division = 4.0f, .velocity = -0.25f, .count = 3 };

    row = trk_pattern_set_row_from_str(&trk->pattern, 8, snare_seq_2);
    row->volume = 0.2f;
    row->pan    = -0.5f;
    row->cells[5].retrig = (Trk_Retrig){.division = 4.0f, .velocity = 0.5f, .count = 4 };
    row->cells[13].retrig = (Trk_Retrig){.division = 4.0f, .velocity = -0.2f, .count = 3 };

    row = trk_pattern_set_row_from_str(&trk->pattern, 9, batrk_seq);
    row->volume = 0.25f;
    row->cells[0].retrig = (Trk_Retrig){.division = 4.0f, .velocity = 0.25f, .count = 4 };

    row = trk_pattern_set_row_from_str(&trk->pattern, 10, batrk_seq_2);
    row->volume = 0.15f;
    row->pitch  = 0.8f;
    row->cells[0].pan = -1.0f;

    row->cells[14].retrig = (Trk_Retrig){.division = 4.0f, .velocity = 0.25f, .count = 4 };
    row->cells[14].pan = -1.0f;

    trk_pattern_print(&trk->pattern);

    return;
}

void trk_module_update(Trk* trk, Ring_Buffer* out)
{
    trk_play_pattern(trk, &trk->pattern, out);
    arena_reset(&trk->tran_arena);

    return;
}
