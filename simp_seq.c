#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "base_ring_buffer.h"

#include "wav.h"
#include "wav_dump_utils.h"

#include "os.h"
#include "os_win32.h"

#include "base_arena.c"
#include "base_string.c"
#include "base_ring_buffer.c"

#include "wav.c"
#include "wav_dump_utils.c"

#include "os.c"

#define OS_WIN32_DLL
#include "os_win32.c"

#include "simp_seq.h"

static bool sound_asset_is_valid(Sound_Asset_Pool* pool, u32 slot_id)
{
    if (slot_id > 0 && 
        slot_id < MAX_SOUND_ASSET_SLOTS && 
        pool->used[slot_id])
    {
        return true;
    }
    
    return false;
}

static u32 sound_asset_add(Sound_Asset_Pool* pool, String filename)
{
    u32 slot_id;

    if (pool->first_free != 0)
    {
        // prioritize freed slot we can reuse
        slot_id = pool->first_free;
        pool->first_free = pool->slot[slot_id].next;

        if (pool->first_free == 0)
        {
            pool->last_free = 0;
        }
    }
    else if (pool->first_unused != 0)
    {
        // no free slots to reuse, but we have unused slots
        slot_id = pool->first_unused++;

    }
    else if (pool->first_slot == 0)
    {
        // no unused slot because the list is empty!
        slot_id = ++pool->first_unused;
        pool->first_unused++;
    }
    else
    {
        // uh oh, we are all full :(

        slot_id = 0;
    }
    
    if (slot_id != 0)
    {
        Sound_Asset_Slot* slot = &pool->slot[slot_id];

        slot->filename = filename;

        slot->prev = pool->last_slot;
        slot->next = 0;

        if (pool->last_slot != 0)
        {
            // update last entry to point to new entry
            pool->slot[pool->last_slot].next = slot_id;
        }
        else
        {
            pool->first_slot = slot_id;
        }

        pool->last_slot = slot_id;
        pool->used[slot_id] = true;
    }

    return slot_id;
}

static void sound_asset_rem(Sound_Asset_Pool* pool, u32 slot_id)
{
    if (sound_asset_is_valid(pool, slot_id))
    {
        // remove from active list
        Sound_Asset_Slot* slot = &pool->slot[slot_id];

        if (slot->prev != 0)
        {
            // connect slot's previous to next
            pool->slot[slot->prev].next = slot->next;
        }
        else
        {
            // slot didn't have a previous so it must be the first
            pool->first_slot = slot->next;
        }

        if (slot->next != 0)
        {
            // connect slot's next to previous
            pool->slot[slot->next].prev = slot->prev;
        }
        else
        {
            // slot didn't have a next so it must be the last
            pool->last_slot = slot->prev;
        }

        pool->used[slot_id] = false;

        // append slot to free list
        slot->next = 0;

        if (pool->last_free != 0)
        {
            // append to end of the list
            pool->slot[pool->last_free].next = slot_id;
        }
        else
        {
            // list is empty, must feel good to be first
            pool->first_free = slot_id;
        }

        pool->last_free = slot_id;
    }

    return;
}

static Sound_Asset_Slot* sound_asset_get(Sound_Asset_Pool* pool, u32 slot_id)
{
    Sound_Asset_Slot* result = &pool->slot[0];

    if (sound_asset_is_valid(pool, slot_id))
    {
        result = &pool->slot[slot_id];
    }

    return result;
}

static Sound_Asset_Slot* sound_asset_find_or_load(Arena* arena, Sound_Asset_Pool* pool, u32 slot_id)
{
    Sound_Asset_Slot* result = &pool->slot[0];

    if (sound_asset_is_valid(pool, slot_id))
    {
        result = &pool->slot[slot_id];

        if (result->data.buffer == 0)
        {
            // load the sound!
            
            String file = os_read_entire_file(result->filename);
            Wav wav     = wav_from_data(arena, file);

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

    return result;
}

static bool sound_instance_is_valid(Sound_Instance_Pool* pool, u32 slot_id)
{
    if (slot_id > 0 && 
        slot_id < MAX_SOUND_INSTANCE_SLOTS && 
        pool->used[slot_id])
    {
        return true;
    }
    
    return false;
}

static u32 sound_instance_add(Sound_Instance_Pool* pool, u32 asset_id)
{
    u32 slot_id;

    if (pool->first_free != 0)
    {
        // prioritize freed slot we can reuse
        slot_id = pool->first_free;
        pool->first_free = pool->slot[slot_id].next;

        if (pool->first_free == 0)
        {
            pool->last_free = 0;
        }
    }
    else if (pool->first_unused != 0)
    {
        // no free slots to reuse, but we have unused slots
        slot_id = pool->first_unused++;

    }
    else if (pool->first_slot == 0)
    {
        // no unused slot because the list is empty!
        slot_id = ++pool->first_unused;
        pool->first_unused++;
    }
    else
    {
        // uh oh, we are all full :(

        slot_id = 0;
    }
    
    if (slot_id != 0)
    {
        Sound_Instance_Slot* slot = &pool->slot[slot_id];

        slot->asset_id = asset_id;
        slot->playhead = 0;
        slot->volume   = 0.5f;
        slot->pan      = 0.0f;
        slot->pitch    = 1.0f;
        slot->loop     = false;

        slot->prev = pool->last_slot;
        slot->next = 0;

        if (pool->last_slot != 0)
        {
            // update last entry to point to new entry
            pool->slot[pool->last_slot].next = slot_id;
        }
        else
        {
            pool->first_slot = slot_id;
        }

        pool->last_slot = slot_id;
        pool->used[slot_id] = true;
    }

    return slot_id;
}

static void sound_instance_rem(Sound_Instance_Pool* pool, u32 slot_id)
{
    if (sound_instance_is_valid(pool, slot_id))
    {
        // remove from active list
        Sound_Instance_Slot* slot = &pool->slot[slot_id];

        if (slot->prev != 0)
        {
            // connect slot's previous to next
            pool->slot[slot->prev].next = slot->next;
        }
        else
        {
            // slot didn't have a previous so it must be the first
            pool->first_slot = slot->next;
        }

        if (slot->next != 0)
        {
            // connect slot's next to previous
            pool->slot[slot->next].prev = slot->prev;
        }
        else
        {
            // slot didn't have a next so it must be the last
            pool->last_slot = slot->prev;
        }

        pool->used[slot_id] = false;

        // append slot to free list
        slot->next = 0;

        if (pool->last_free != 0)
        {
            // append to end of the list
            pool->slot[pool->last_free].next = slot_id;
        }
        else
        {
            // list is empty, must feel good to be first
            pool->first_free = slot_id;
        }

        pool->last_free = slot_id;
    }

    return;
}

static Sound_Instance_Slot* sound_instance_get(Sound_Instance_Pool* pool, u32 slot_id)
{
    Sound_Instance_Slot* result = &pool->slot[0];

    if (sound_instance_is_valid(pool, slot_id))
    {
        result = &pool->slot[slot_id];
    }

    return result;
}

static void sequence_init(Sequence* sequence, f32 bpm, u32 num_steps)
{
    sequence->bpm = bpm;

    sequence->volume = 1.0;
    sequence->pitch  = 1.0;
    sequence->pan    = 0.0;

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
            // str_eat_whitespace(&row_str);
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

    // fflush(stdout);
    // printf("\033[%dA", sequence->row_count + 2);
}


static void ss_init(Simp_Seq_State* ss)
{
    // why am I doing this - not sure.
    u8* dest = (u8*)ss;
    for (u32 byte_index = 0; byte_index < sizeof(*ss); byte_index++)
    {
        *dest++ = 0;
    }

    arena_alloc(&ss->perm_arena, 16*1024*1024);
    arena_alloc(&ss->tran_arena, 4*1024);

    return;
}

static u32 ss_play_sound(Simp_Seq_State* ss, u32 asset_id)
{
    u32 instance_id = 0;

    Sound_Asset_Slot* asset = sound_asset_find_or_load(&ss->perm_arena, &ss->asset_pool, asset_id);

    if (asset->data.buffer != 0)
    {
        instance_id = sound_instance_add(&ss->instance_pool, asset_id);
    }

    return instance_id;
}

static void ss_stop_sound(Simp_Seq_State* ss, u32 instance_id)
{
    sound_instance_rem(&ss->instance_pool, instance_id);

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
                        Sound_Instance_Slot* sound = sound_instance_get(&ss->instance_pool, sound_id);

                        sound->volume = row->volume * cell->volume;
                        sound->pan    = row->pan;
                        sound->pitch  = row->pitch;

                        // printf("playing  sound! sound_id: %u, asset_id: %u, playhead: %u\n", sound_id, sequence->rows[row_index].asset_id, sound->playhead);
                    }
                }

                sequence->playhead++;
            }
        }
        else
        {
            // TODO[nr] @scale

            // hit the end of the sequence
            ss->total_samples_mixed = 0;
            sequence->playhead = 0;

            // printf("loop!\n");
            return;
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

    for (u32 sound_id = ss->instance_pool.first_slot; sound_id != 0; )
    {
        Sound_Instance_Slot* sound = sound_instance_get(&ss->instance_pool, sound_id);
        Sound_Asset_Slot*    asset = sound_asset_get(&ss->asset_pool, sound->asset_id);

        u32 byte_index       = sound->playhead * bytes_per_sample;
        f32* asset_data      = (f32*)&asset->data.buffer[byte_index];
        u32 samples_in_asset = asset->data.size / bytes_per_sample; 

        // printf("sound_id: %u, bytes_index: %u, bytes_in_asset: %u, playhead: %u, samples_in_asset: %u\n", sound_id, byte_index, asset->data.size, sound->playhead, samples_in_asset);

        mix_buffer = ss->mix_buffer;

        for (u32 mix_index = 0; mix_index < samples_to_mix; mix_index += 2)
        {
            if (sound->playhead < samples_in_asset)
            {
                // TODO[nr] @study: currently doing equal power panning, but that makes center quieter...
#if 0
                f32 pan_0 = (1 - sound->pan);
                f32 pan_1 = (sound->pan);
#else
                f32 pan_0 = sound->pan < 0 ? 1.0f : 1 - sound->pan;
                f32 pan_1 = sound->pan > 0 ? 1.0f : 1 + sound->pan;
#endif

                mix_buffer[0] += (asset_data[0] * sound->volume * pan_0);
                mix_buffer[1] += (asset_data[1] * sound->volume * pan_1);

                mix_buffer += 2;
                asset_data += 2;

                sound->playhead += 2;
            }
            else
            {
                break;
            }
        }

        u32 next_sound_id = sound_instance_get(&ss->instance_pool, sound_id)->next;

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

void ss_update(Simp_Seq_State* ss, Ring_Buffer* out)
{
    if (!ss->initialized)
    {
        printf("initializing simp seq!\n");

        // initialize memory
        ss_init(ss);

        // add assets
        ss->sound_asset_id = sound_asset_add(&ss->asset_pool, STR_LIT("test_track.wav"));
        ss->pan_direction  = -1;

        ss->sequence.bpm        = 180;
        ss->sequence.volume     = 1.0;
        ss->sequence.pitch      = 1.0;
        ss->sequence.pan        = 0.0;
        ss->sequence.row_count  = 0;
        ss->sequence.cell_count = 16;

        sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->asset_pool, STR_LIT("../data/vocals1.wav"));
        sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->asset_pool, STR_LIT("../data/vocals5.wav"));
        sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->asset_pool, STR_LIT("../data/stage_grand_g.wav"));
        sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->asset_pool, STR_LIT("../data/kick.wav"));
        sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->asset_pool, STR_LIT("../data/ch.wav"));
        sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->asset_pool, STR_LIT("../data/oh.wav"));
        sequence_add_row(&ss->sequence)->asset_id = sound_asset_add(&ss->asset_pool, STR_LIT("../data/snare.wav"));

        ss->initialized = true;
    }

    ss->sequence.bpm        = 155;
    ss->sequence.volume     = 1.0;
    ss->sequence.pitch      = 1.0;
    ss->sequence.pan        = 0.0;
    ss->sequence.cell_count = 16;

    String voice1_seq = STR_LIT("0000000000000000");
    String voice2_seq = STR_LIT("1000000010001000");
    String piano_g    = STR_LIT("0000000000000000");
    String kick_seq   = STR_LIT("1010000010001001");
    String ch_seq     = STR_LIT("0000010100110010");
    String oh_seq     = STR_LIT("1000000010000100");
    String snare_seq  = STR_LIT("0000100000001000");

    Sequence_Row* row;

    row = sequence_set_row_from_str(&ss->sequence, 0, voice1_seq);
    row->volume = 0.5f;

    row = sequence_set_row_from_str(&ss->sequence, 1, voice2_seq);
    row->volume = 0.5f;

    row = sequence_set_row_from_str(&ss->sequence, 2, piano_g);
    row->volume = 0.25f;

    for (u32 cell_index = 0; cell_index < ss->sequence.cell_count; ++cell_index)
    {
        if (cell_index % 2 == 0)
        {
            row->cells[cell_index].volume = 0.25f;
            row->cells[cell_index].pan = 0.7f;
        }
    }

    row = sequence_set_row_from_str(&ss->sequence, 3, kick_seq);
    row = sequence_set_row_from_str(&ss->sequence, 4, ch_seq);
    row = sequence_set_row_from_str(&ss->sequence, 5, oh_seq);
    row = sequence_set_row_from_str(&ss->sequence, 6, snare_seq);

    // sequence_print(&ss->sequence);

    ss_play_sequence(ss, &ss->sequence);

    ss_mix(ss, out);
    
    return;
}
