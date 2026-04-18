#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "base_ring_buffer.h"

#include "slot_pool.h"

#include "wav.h"
#include "wav_dump_utils.h"

#include "os.h"
#include "os_win32.h"

#include "base_arena.c"
#include "base_string.c"
#include "base_ring_buffer.c"

#include "slot_pool.c"

#include "wav.c"
#include "wav_dump_utils.c"

#include "os.c"

#define OS_WIN32_DLL
#include "os_win32.c"

#include "simp_seq.h"

static f32 lerp(f32 a, f32 b, f32 t)
{
    return (a * (1.0f - t)) + (b * t);
}

static u32 sound_asset_add(Arena* arena, Sound_Asset_Slop* asset_slop, String filename)
{
    printf("\nAttempting to add asset %s\n", filename.data);

    // see if asset already exists and return its id if it does
    for (u32 slot_id = asset_slop->slop.first_slot; slot_id != 0; slot_id = asset_slop->slop.slots[slot_id].next)
    {
        Sound_Asset* asset = &asset_slop->assets[slot_id];

        if (str_compare(filename, asset->filename))
        {
            printf("  Already exists! ID: %u\n", slot_id);
            return slot_id;
        }
    }

    u32 slot_id = slop_slot_add(&asset_slop->slop);

    if (slot_id != 0)
    {
        Sound_Asset* asset = &asset_slop->assets[slot_id];

        asset->filename = str_copy(arena, filename);
        printf("  Success! ID: %u\n", slot_id);
    }

    return slot_id;
}

static void sound_asset_rem(Sound_Asset_Slop* asset_slop, u32 slot_id)
{
    if (slop_slot_is_valid(&asset_slop->slop, slot_id))
    {
        // clear data
        Sound_Asset* asset = &asset_slop->assets[slot_id];
        *asset = (Sound_Asset){0};

        // remove from active slot pool
        slop_slot_rem(&asset_slop->slop, slot_id);
    }

    return;
}

static Sound_Asset* sound_asset_get(Sound_Asset_Slop* asset_slop, u32 slot_id)
{
    Sound_Asset* result = &asset_slop->assets[0];

    if (slop_slot_is_valid(&asset_slop->slop, slot_id))
    {
        result = &asset_slop->assets[slot_id];
    }

    return result;
}

static Sound_Asset* sound_asset_find_or_load(Arena* arena, Sound_Asset_Slop* asset_slop, u32 slot_id)
{
    Sound_Asset* result = &asset_slop->assets[0];

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

static u32 sound_add(Sound_Slop* sound_slop, u32 asset_id)
{
    u32 slot_id = slop_slot_add(&sound_slop->slop);

    if (slot_id != 0)
    {
        Sound* sound = &sound_slop->sounds[slot_id];
        sound->asset_id = asset_id;
        sound->playhead = 0;
        sound->volume   = 0.5f;
        sound->pitch    = 1.0f;
        sound->pan      = 0.0f;
        sound->loop     = false;
    }

    return slot_id;
}

static void sound_rem(Sound_Slop* sound_slop, u32 slot_id)
{
    if (slop_slot_is_valid(&sound_slop->slop, slot_id))
    {
        // clear data
        Sound* sound = &sound_slop->sounds[slot_id];
        *sound = (Sound){0};

        // remove from active slot pool
        slop_slot_rem(&sound_slop->slop, slot_id);
    }

    return;
}

static Sound* sound_get(Sound_Slop* sound_slop, u32 slot_id)
{
    Sound* result = &sound_slop->sounds[0];

    if (slop_slot_is_valid(&sound_slop->slop, slot_id))
    {
        result = &sound_slop->sounds[slot_id];
    }

    return result;
}

static void sequence_init(Sequence* sequence, f32 bpm, u32 num_steps)
{
    sequence->bpm = bpm;

    sequence->volume = 1.0f;
    sequence->pitch  = 1.0f;
    sequence->pan    = 0.0f;

    sequence->cell_count = num_steps;

    return;
}

static void sequence_clear(Sequence* sequence)
{
    sequence->bpm = 0.0f;

    sequence->volume = 0.0;
    sequence->pitch  = 0.0;
    sequence->pan    = 0.0;

    sequence->cell_count = 0;
    sequence->row_count  = 0;

    return;
}

static Sequence_Row* sequence_add_row(Sequence* sequence)
{
    Sequence_Row* result = 0;

    if (sequence->row_count < MAX_SEQUENCE_ROWS)
    {
        u32 row_index = sequence->row_count;
        result = &sequence->rows[row_index];

        result->volume = 1.0f;
        result->pitch  = 1.0f;
        result->pan    = 0.0f;

        sequence->row_count += 1;
    }

    return result;
}

static Sequence_Row* sequence_set_row_from_str(Sequence* sequence, u32 row_id, String row_str)
{
    Sequence_Row* row = 0;

    if (row_id < sequence->row_count)
    {
        row = &sequence->rows[row_id];

        u32 cell_index = 0; 
        while (*row_str.data != 0 && cell_index < sequence->cell_count)
        {
            str_eat_whitespace(&row_str);
            String cell_str = str_advance(&row_str, 1);

            switch (*cell_str.data)
            {
                case '0':
                {
                    row->cells[cell_index++] = (Sequence_Cell){0};
                    break;
                }
                case '1':
                {
                    row->cells[cell_index++] = (Sequence_Cell){true, 1.0, 1.0, 0.0};
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

static Sequence_Row* sequence_add_row_from_str(Sequence* sequence, String row_str)
{
    Sequence_Row* row = sequence_add_row(sequence);

    if (row != 0)
    {
        row = sequence_set_row_from_str(sequence, sequence->row_count - 1, row_str);
    }

    return row;
}

static Sequence_Row* sequence_add_row_from_int(Sequence* sequence, u32 row_int)
{
    Sequence_Row* row = sequence_add_row(sequence);

    if (row != 0)
    {
        u32 cell_index = 0;
        row_int <<= (MAX_SEQUENCE_CELLS - sequence->cell_count);

        while (row_int > 0 && cell_index < sequence->cell_count)
        {
            u32 cell_int = (row_int & (0x80000000)) >> (MAX_SEQUENCE_CELLS - 1);

            switch (cell_int)
            {
               case 0:
               {
                   row->cells[cell_index++] = (Sequence_Cell){0};
                   break;
               }
               case 1:
               {
                   row->cells[cell_index++] = (Sequence_Cell){true, 1.0, 1.0, 0.0};
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
static void sequence_print(Sequence* sequence)
{
    printf("   |");
    for (u32 cell_index = 0; cell_index < sequence->cell_count; ++cell_index)
    {
        printf("C%.2u|", cell_index);
    }
    printf("\n");

    for (u32 row_index = 0; row_index < sequence->row_count; ++row_index)
    {
        printf("R%.2u|", row_index);
        for (u32 cell_index = 0; cell_index < sequence->cell_count; ++cell_index)
        {
            if (sequence->rows[row_index].cells[cell_index].active)
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
    for (u32 cell_index = 0; cell_index < sequence->cell_count; ++cell_index)
    {
        printf("---|");
    }
    printf("\n");
}

static void ss_init(Simp_Seq_State* ss)
{
    u8* dest = (u8*)ss;
    for (u32 byte_index = 0; byte_index < sizeof(*ss); byte_index++)
    {
        *dest++ = 0;
    }

    arena_alloc(&ss->perm_arena, 16*1024*1024);
    arena_alloc(&ss->tran_arena, 4*1024);

    return;
}

static void sequence_reset(Simp_Seq_State* ss)
{
    ss->sequence.cell_count = 0;
    ss->sequence.row_count = 0;

    return;
}

static u32 ss_play_sound(Simp_Seq_State* ss, u32 asset_id)
{
    u32 sound_id = 0;

    Sound_Asset* asset = sound_asset_find_or_load(&ss->perm_arena, &ss->asset_slop, asset_id);

    if (asset->data.buffer != 0)
    {
        sound_id = sound_add(&ss->sound_slop, asset_id);
    }

    return sound_id;
}

static void ss_stop_sound(Simp_Seq_State* ss, u32 sound_id)
{
    sound_rem(&ss->sound_slop, sound_id);

    return;
}


static void ss_play_sequence(Simp_Seq_State* ss, Sequence* sequence)
{
    if (sequence->bpm > 0)
    {
        f32 beats_per_second   = sequence->bpm / 60;
        f32 samples_per_second = (f32)SUPPORTED_SAMPLE_RATE;

        u32 samples_per_beat = (u32)(samples_per_second / beats_per_second);

        u32 total_samples_in_sequence = samples_per_beat * sequence->cell_count;

        if (ss->total_samples_mixed < total_samples_in_sequence)
        {
            u32 cell_index = ss->total_samples_mixed / samples_per_beat;

            if (cell_index >= sequence->playhead && cell_index < MAX_SEQUENCE_CELLS)
            {
                // next column needs to play!
                // printf("cell_index = %u, playhead = %u\n", cell_index, sequence->playhead);

                for (u32 row_index = 0; row_index < sequence->row_count; ++row_index)
                {
                    Sequence_Row*  row  = &sequence->rows[row_index];
                    Sequence_Cell* cell = &row->cells[cell_index];

                    if (cell->active)
                    {
                        u32 sound_id = ss_play_sound(ss, row->asset_id);
                        Sound* sound = sound_get(&ss->sound_slop, sound_id);

                        sound->volume = sequence->volume * row->volume * cell->volume;

                        f32 pan     = row->pan + cell->pan;
                        pan         = pan < -1.0f ? pan = -1.0f : pan > 1.0f ? pan = 1.0f : pan;
                        sound->pan  = pan;

                        sound->pitch  = row->pitch * cell->pitch;

                        // printf("playing  sound! sound_id: %u, asset_id: %u, playhead: %u\n", sound_id, sequence->rows[row_index].asset_id, sound->playhead);
                    }
                }

                sequence->playhead++;
            }
        }
        else
        {
            // hit the end of the sequence

            if (sequence->loop)
            {
                ss->total_samples_mixed = 0;
                sequence->playhead = 0;
            }
        }
    }
}

static void ss_mix(Simp_Seq_State* ss, Ring_Buffer* out)
{
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // TODO[nr] @better: this whole thing depends on the WAV being f32 48KHz stereo!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    u32 bytes_available = (u32)out->amount_free;

    u32 bytes_per_sample = sizeof(*ss->mix_buffer);

    u32 samples_available = bytes_available / bytes_per_sample;
    u32 samples_to_mix = 9600;
    samples_to_mix = samples_available < samples_to_mix ? samples_available : samples_to_mix;

    // clear buffer
    f32* mix_buffer = ss->mix_buffer;
    for (u32 mix_index = 0; mix_index < samples_to_mix; ++mix_index)
    {
        *mix_buffer++ = 0;
    }

    for (u32 sound_id = ss->sound_slop.slop.first_slot; sound_id != 0; )
    {
        Sound*       sound = sound_get(&ss->sound_slop, sound_id);
        Sound_Asset* asset = sound_asset_get(&ss->asset_slop, sound->asset_id);

        u32 byte_index       = sound->playhead * bytes_per_sample;
        f32* asset_data      = (f32*)&asset->data.buffer[byte_index];
        u32 samples_in_asset = asset->data.size / bytes_per_sample; 

        // printf("sound_id: %u, bytes_index: %u, bytes_in_asset: %u, playhead: %u, samples_in_asset: %u\n", sound_id, byte_index, asset->data.size, sound->playhead, samples_in_asset);

        mix_buffer = ss->mix_buffer;

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

        for (f32 frame_pos = 0; frame_pos < (f32)samples_to_mix/2; frame_pos += d_sample)
        // for (u32 mix_index = 0; mix_index < samples_to_mix; mix_index += 2)
        {
            u32 frame_index = (u32)(frame_pos);
            sample_index = frame_index * 2;

            if (sound->playhead + sample_index < samples_in_asset)
            {
                f32 frac = frame_pos - (f32)frame_index;

                f32 sample_0 = asset_data[sample_index];
                f32 sample_1 = asset_data[sample_index+1];

                if (sound->playhead + sample_index + 2 < samples_in_asset)
                {
                    f32 sample_2 = asset_data[sample_index + 2];
                    f32 sample_3 = asset_data[sample_index + 3];

                    sample_0 = lerp(sample_0, sample_2, frac);
                    sample_1 = lerp(sample_1, sample_3, frac);
                }               

                mix_buffer[0] += (sample_0 * sound->volume * pan_0);
                mix_buffer[1] += (sample_1 * sound->volume * pan_1);
                mix_buffer += 2;
            }
            else
            {
                break;
            }
        }

        sound->playhead += sample_index;

        u32 next_sound_id = ss->sound_slop.slop.slots[sound_id].next;

        if (sound->playhead >= samples_in_asset)
        {
            if (sound->loop)
            {
                sound->playhead = 0;
            }
            else
            {
                // printf("stopping sound! sound_id: %u, asset_id: %u, playhead: %u\n", sound_id, sound->asset_id, sound->playhead);
                ss_stop_sound(ss, sound_id);
            }
        }

        sound_id = next_sound_id;

    }

    ss->total_samples_mixed += samples_to_mix;

    rb_write(out, (u8*)ss->mix_buffer, samples_to_mix*bytes_per_sample);

    // u32 bytes_written = rb_write(out, (u8*)ss->mix_buffer, samples_to_mix*bytes_per_sample);
    // printf("samples written: %u\n", bytes_written / 4);

    return;
}

void ss_post_load(Simp_Seq_State* ss)
{
    // initialize memory
    ss_init(ss);
    return;
}

void ss_post_reload(Simp_Seq_State* ss)
{
    sequence_reset(ss);

    // add assets
    sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->perm_arena, &ss->asset_slop, STR_LIT("../data/vocals2.wav"));
    sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->perm_arena, &ss->asset_slop, STR_LIT("../data/vocals4.wav"));
    sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->perm_arena, &ss->asset_slop, STR_LIT("../data/ch.wav"));
    sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->perm_arena, &ss->asset_slop, STR_LIT("../data/kick.wav"));
    sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->perm_arena, &ss->asset_slop, STR_LIT("../data/ch.wav"));
    sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->perm_arena, &ss->asset_slop, STR_LIT("../data/oh.wav"));
    sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->perm_arena, &ss->asset_slop, STR_LIT("../data/snare.wav"));

    ss->sequence.bpm        = 103;
    ss->sequence.volume     = 0.8f;
    ss->sequence.pitch      = 1.0f;
    ss->sequence.pan        = 0.0f;
    ss->sequence.cell_count = 16;

    ss->sequence.loop = true;

#if 1
    String voice1_seq = STR_LIT("0000 0000 0000 0001");
    String voice2_seq = STR_LIT("0000 0001 0000 0000");
    String ch_0_seq   = STR_LIT("0000 0000 0000 0000");
    String kick_seq   = STR_LIT("0000 0000 0000 0000");
    String ch_seq     = STR_LIT("0000 0000 0000 0000");
    String oh_seq     = STR_LIT("0000 0000 0000 0000");
    String snare_seq  = STR_LIT("0000 0000 0000 0000");
#else

    String voice1_seq = STR_LIT("0000 0000 0000 0001");
    String voice2_seq = STR_LIT("0000 0001 0000 0000");
    String ch_0_seq   = STR_LIT("0101 0010 0101 0010");
    String kick_seq   = STR_LIT("1000 0000 1010 0000");
    String ch_seq     = STR_LIT("1111 1111 1111 1111");
    String oh_seq     = STR_LIT("0010 1100 0010 1001");
    String snare_seq  = STR_LIT("0000 1000 0000 1000");
#endif

    Sequence_Row* row;

    row = sequence_set_row_from_str(&ss->sequence, 0, voice1_seq);
    row->volume = 0.1f;
    row->pitch  = 0.8f;

    row = sequence_set_row_from_str(&ss->sequence, 1, voice2_seq);
    row->volume = 0.1f;

    row = sequence_set_row_from_str(&ss->sequence, 2, ch_0_seq);
    row->volume = 0.75f;

    for (u32 cell_index = 0; cell_index < ss->sequence.cell_count; ++cell_index)
    {
        if (cell_index % 2 == 0)
        {
            row->cells[cell_index].volume = 0.5f;
            row->cells[cell_index].pan = 0.9f;
        }
    }

    row = sequence_set_row_from_str(&ss->sequence, 3, kick_seq);
    row->volume = 0.3f;

    row = sequence_set_row_from_str(&ss->sequence, 4, ch_seq);
    row->volume = 0.8f;

    row = sequence_set_row_from_str(&ss->sequence, 5, oh_seq);
    row->volume = 0.4f;

    row = sequence_set_row_from_str(&ss->sequence, 6, snare_seq);
    row->volume = 0.2f;

    sequence_print(&ss->sequence);

    return;
}

void ss_update(Simp_Seq_State* ss, Ring_Buffer* out)
{
    ss_play_sequence(ss, &ss->sequence);
    ss_mix(ss, out);
    
    return;
}
