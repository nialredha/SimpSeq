static bool slop_slot_is_valid(Slop* slop, u32 slot_id)
{
    if (slot_id > 0 && 
        slot_id < SLOP_MAX_SLOTS && 
        slop->slots[slot_id].used)
    {
        return true;
    }
    
    return false;
}

static u32 slop_slot_add(Slop* slop)
{
    // printf("\nAttempting to add slot...\n");

    u32 slot_id = 0;

    if (slop->first_unused != 0 && slop->first_unused < SLOP_MAX_SLOTS)
    {
        // no free slots to reuse, but we have unused slots
        slot_id = slop->first_unused++;
    }
    else if (slop->first_free != 0)
    {
        // prioritize freed slot we can reuse
        slot_id = slop->first_free;
        slop->first_free = slop->slots[slot_id].next;

        if (slop->first_free == 0)
        {
            slop->last_free = 0;
        }
    }
    else if (slop->first_slot == 0)
    {
        // no unused slot because the list is empty!
        slot_id = ++slop->first_unused;
        slop->first_unused++;
    }
    else
    {
        // uh oh, we are all full :(
        assert(false);
        slot_id = 0;
    }
    
    if (slot_id != 0)
    {
        Slop_Slot* slot = &slop->slots[slot_id];

        slot->next = 0;
        slot->prev = slop->last_slot;
        slot->used = true;

        if (slop->last_slot != 0)
        {
            // update last entry to point to new entry
            slop->slots[slop->last_slot].next = slot_id;
        }
        else
        {
            slop->first_slot = slot_id;
        }

        slop->last_slot = slot_id;

        // printf("  Success! ID: %u\n", slot_id);
    }

    return slot_id;
}

static void slop_slot_rem(Slop* slop, u32 slot_id)
{
    // printf("\nAttempting to rem slot %u...\n", slot_id);

    if (slop_slot_is_valid(slop, slot_id))
    {
        // remove from active list
        Slop_Slot* slot = &slop->slots[slot_id];

        if (slot->prev != 0)
        {
            // connect slot's previous to next
            slop->slots[slot->prev].next = slot->next;
        }
        else
        {
            // slot didn't have a previous so it must be the first
            slop->first_slot = slot->next;
        }

        if (slot->next != 0)
        {
            // connect slot's next to previous
            slop->slots[slot->next].prev = slot->prev;
        }
        else
        {
            // slot didn't have a next so it must be the last
            slop->last_slot = slot->prev;
        }


        // append slot to free list
        slot->next = 0;
        slot->used = false;

        if (slop->last_free != 0)
        {
            // append to end of the list
            slot->prev = slop->last_free;
            slop->slots[slop->last_free].next = slot_id;
        }
        else
        {
            // list is empty, must feel good to be first
            slot->prev = 0;
            slop->first_free = slot_id;
        }

        slop->last_free = slot_id;

        // printf("  Success!\n");
    }

    return;
}
