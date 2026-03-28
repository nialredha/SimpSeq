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
#include "os_win32.c"

#define SUPPORTED_FORMAT_TAG    (Wav_Format_Tag_IEE_FLOAT)
#define SUPPORTED_BIT_DEPTH     (32)
#define SUPPORTED_CHANNEL_COUNT (2)
#define SUPPORTED_SAMPLE_RATE   (48000)

#define MAX_SOUND_ASSET_SLOTS    (32)
#define MAX_SOUND_INSTANCE_SLOTS (64)

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
    Arena perm_arena;
    Arena tran_arena;

    Sound_Asset_Pool    asset_pool;
    Sound_Instance_Pool instance_pool;

    float mix_buffer[9600]; // TODO[nr] @better: assuming 100ms 48KHz stereo 
} Sound_System;

bool sound_asset_is_valid(Sound_Asset_Pool* pool, u32 slot_id)
{
    if (slot_id > 0 && 
        slot_id < MAX_SOUND_ASSET_SLOTS && 
        pool->used[slot_id])
    {
        return true;
    }
    
    return false;
}

u32 sound_asset_add(Sound_Asset_Pool* pool, String filename)
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

void sound_asset_rem(Sound_Asset_Pool* pool, u32 slot_id)
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

Sound_Asset_Slot* sound_asset_get(Sound_Asset_Pool* pool, u32 slot_id)
{
    Sound_Asset_Slot* result = &pool->slot[0];

    if (sound_asset_is_valid(pool, slot_id))
    {
        result = &pool->slot[slot_id];
    }

    return result;
}

Sound_Asset_Slot* sound_asset_find_or_load(Arena* arena, Sound_Asset_Pool* pool, u32 slot_id)
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

            result->format = format;
            result->data   = data;

            free(file.data);
        }
    }

    return result;
}

bool sound_instance_is_valid(Sound_Instance_Pool* pool, u32 slot_id)
{
    if (slot_id > 0 && 
        slot_id < MAX_SOUND_INSTANCE_SLOTS && 
        pool->used[slot_id])
    {
        return true;
    }
    
    return false;
}

u32 sound_instance_add(Sound_Instance_Pool* pool, u32 asset_id)
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
        slot->volume   = 1.0;
        slot->pan      = 0.0;
        slot->pitch    = 1.0;
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

void sound_instance_rem(Sound_Instance_Pool* pool, u32 slot_id)
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

Sound_Instance_Slot* sound_instance_get(Sound_Instance_Pool* pool, u32 slot_id)
{
    Sound_Instance_Slot* result = &pool->slot[0];

    if (sound_instance_is_valid(pool, slot_id))
    {
        result = &pool->slot[slot_id];
    }

    return result;
}

void ss_init(Sound_System* ss)
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

u32 ss_play_sound(Sound_System* ss, u32 asset_id)
{
    u32 instance_id = 0;

    Sound_Asset_Slot* asset = sound_asset_find_or_load(&ss->perm_arena, &ss->asset_pool, asset_id);

    if (asset->data.buffer != 0)
    {
        instance_id = sound_instance_add(&ss->instance_pool, asset_id);
    }

    return instance_id;
}

void ss_stop_sound(Sound_System* ss, u32 instance_id)
{
    sound_instance_rem(&ss->instance_pool, instance_id);

    return;
}

void ss_mix(Sound_System* ss, Ring_Buffer* out)
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

    for (u32 sound_id = ss->instance_pool.first_slot;
         sound_id != 0;
         sound_id = sound_instance_get(&ss->instance_pool, sound_id)->next)
    {
        Sound_Instance_Slot* sound = sound_instance_get(&ss->instance_pool, sound_id);
        Sound_Asset_Slot*    asset = sound_asset_get(&ss->asset_pool, sound->asset_id);

        u32 byte_index       = sound->playhead * bytes_per_sample;
        f32* asset_data      = (f32*)&asset->data.buffer[byte_index];
        u32 samples_in_asset = asset->data.size / bytes_per_sample; 

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

                *mix_buffer++ += (*asset_data++ * sound->volume * pan_0);
                *mix_buffer++ += (*asset_data++ * sound->volume * pan_1);

                sound->playhead += 2;
            }
            else
            {
                if (sound->loop)
                {
                    sound->playhead = 0;
                }
                else
                {
                    ss_stop_sound(ss, sound_id);
                }

                samples_to_mix = mix_index;
            }
        }

    }

    rb_write(out, (u8*)ss->mix_buffer, samples_to_mix*bytes_per_sample);

    return;
}

typedef struct
{
    Ring_Buffer* rb;
} User_Data;

void audio_callback(Wav_Format* format, void* user_data, u32 num_frames_needed, void* output)
{
    User_Data*   ud  = (User_Data*)user_data;
    Ring_Buffer* rb  = ud->rb;

    u32 bytes_needed  = (num_frames_needed * format->num_channels) * (format->bits_per_sample / 8);
    rb_read(rb, (u8*)output, bytes_needed);
}

s32 main2(s32 arg_count, char** args)
{
    (void)arg_count;
    (void)args;

    Sound_System ss = {0};
    ss_init(&ss);

    u32 atwl_aid = sound_asset_add(&ss.asset_pool, STR_LIT("test_track.wav"));

    u32 atwl_iid = ss_play_sound(&ss, atwl_aid);

    Sound_Instance_Slot* atwl = sound_instance_get(&ss.instance_pool, atwl_iid);
    atwl->volume = 0.5f;
    atwl->pan    = -1.0f;
    atwl->loop   = true;

    u32 atwl2_iid = ss_play_sound(&ss, atwl_aid);
    Sound_Instance_Slot* atwl2 = sound_instance_get(&ss.instance_pool, atwl2_iid);
    atwl2->volume = 0.5f;
    atwl2->pan    = 1.0f;
    atwl2->loop   = false;

    OS_Audio_Device device = {0};
    Ring_Buffer rb = rb_create(9600*4); // TODO[nr] @better

    Sound_Asset_Slot* atwl_asset = sound_asset_get(&ss.asset_pool, atwl_aid);

    User_Data    user_data = { &rb };
    OS_Audio_Config config = { atwl_asset->format, audio_callback, &user_data };

    s32 result = 0;

    result = os_win32_audio_init(&device, &config);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to init audio client!\n");
        return result;
    }

    result = os_win32_audio_start(&device);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to start audio client!\n");
        return result;
    }
    
    s32 sign = 1;

    while (true) 
    {
        if      (atwl->pan > 1.0) { sign = -1; }
        else if (atwl->pan < -1.0){ sign =  1; }

        atwl->pan += (sign*0.1f);

        ss_mix(&ss, &rb);
        os_sleep_ms(50);
    }

    // stop playing device
    result = os_win32_audio_stop(&device);
    if (result < 0)
    {
        fprintf(stderr, "ERROR: failed to stop audio client!\n");
        return result;
    }

    os_win32_audio_deinit(&device);

    return 0;
}
