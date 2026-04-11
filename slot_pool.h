#ifndef SLOT_POOL_H
#define SLOT_POOL_H

#define SLOP_MAX_SLOTS (256)

typedef struct
{
    u32 next;
    u32 prev;

    bool used;
} Slop_Slot;

typedef struct
{
    u32 first_slot;
    u32 last_slot;

    u32 first_free;
    u32 last_free;

    u32 first_unused;

    Slop_Slot slots[SLOP_MAX_SLOTS];
} Slop;

static u32  slop_slot_add      (Slop* slop);
static void slop_slot_rem      (Slop* slop, u32 slot_id);
static bool slop_slot_is_valid (Slop* slop, u32 slot_id);

#endif // SLOT_POOL_H
