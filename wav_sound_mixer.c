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

        // TODO[nr] @probably: zero out all the other members of the struct
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

            result->format = wav_get_format(&wav.sub_chunks, file);
            result->data   = wav_get_data(arena, &wav.sub_chunks, file);

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

        // TODO[nr] @probably: zero out all the other members of the struct
        slot->asset_id = asset_id;

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

void ss_init(Sound_System* sound_system)
{
    // why am I doing this - not sure.
    u8* dest = (u8*)sound_system;
    for (u32 byte_index = 0; byte_index < sizeof(*sound_system); byte_index++)
    {
        *dest++ = 0;
    }

    arena_alloc(&sound_system->perm_arena, 4*1024*1024);
    arena_alloc(&sound_system->tran_arena, 4*1024);

    return;
}

u32 ss_play_sound(Sound_System* sound_system, u32 asset_id)
{
    u32 instance_id = 0;

    Sound_Asset_Slot* asset = sound_asset_find_or_load(&sound_system->perm_arena, &sound_system->asset_pool, asset_id);

    if (asset->data.buffer != 0)
    {
        instance_id = sound_instance_add(&sound_system->instance_pool, asset_id);
    }

    return instance_id;
}

void ss_stop_sound(Sound_System* sound_system, u32 instance_id)
{
    sound_instance_rem(&sound_system->instance_pool, instance_id);

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

    Arena arena = {0};
    arena_alloc(&arena, 4*1024*1024);

    Sound_System ss = {0};
    ss_init(&ss);

    u32 kick_aid = sound_asset_add(&ss.asset_pool, STR_LIT("808k_f32.wav"));
    u32 atwl_aid = sound_asset_add(&ss.asset_pool, STR_LIT("atwl_f32.wav"));

    printf("kick_aid = %u, atwl_aid = %u\n", kick_aid, atwl_aid);

    u32 kick_iid = ss_play_sound(&ss, kick_aid);
    u32 atwl_iid = ss_play_sound(&ss, atwl_aid);

    printf("kick_iid = %u, atwl_iid = %u\n", kick_iid, atwl_iid);

    ss_stop_sound(&ss, kick_iid);
    ss_stop_sound(&ss, atwl_iid);

    kick_iid = ss_play_sound(&ss, kick_aid);
    atwl_iid = ss_play_sound(&ss, atwl_aid);

    printf("kick_iid = %u, atwl_iid = %u\n", kick_iid, atwl_iid);

    u32 kick_2_iid = ss_play_sound(&ss, kick_aid);
    u32 atwl_2_iid = ss_play_sound(&ss, atwl_aid);

    printf("kick_2_iid = %u, atwl_2_iid = %u\n", kick_2_iid, atwl_2_iid);
    ss_stop_sound(&ss, kick_2_iid);

    u32 kick_3_iid = ss_play_sound(&ss, kick_aid);
    u32 atwl_3_iid = ss_play_sound(&ss, atwl_aid);

    printf("kick_3_iid = %u, atwl_3_iid = %u\n", kick_3_iid, atwl_3_iid);
    ss_stop_sound(&ss, atwl_2_iid);
    ss_stop_sound(&ss, kick_3_iid);
    ss_stop_sound(&ss, atwl_3_iid);

    return 0;
}
