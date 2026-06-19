//
// arena
//

// allocate/deallocate
static void arena_alloc(Arena* arena, u32 size)
{
    arena->base = (u8*)malloc(size);
    if (arena->base != 0)
    {
        arena->size = size;
        arena->used = 0;
    }
}

static void arena_realloc(Arena* arena, u32 new_size)
{
    arena->base = (u8*)realloc(arena->base, new_size);
    if (arena->base != 0)
    {
        arena->size = new_size;
    }
}

static void arena_free(Arena* arena)
{
    if (arena->base != 0)
    {
        free(arena->base);
        arena->base = 0;
    }

    arena->size = 0;
    arena->used = 0;
}

// update
static void arena_reset(Arena* arena)
{
    arena->used = 0;
}

static void* arena_push(Arena* arena, u32 size)
{
    assert(arena->used + size <= arena->size);

    void* result = arena->base + arena->used;
    arena->used += size;

    return result;
}

static void* arena_resize(Arena* arena, void* ptr, u32 old_size, u32 new_size)
{
    // memory must be the top allocation
    assert((u8*)ptr + old_size == arena->base + arena->used);

    // amount we are growing by must fit inside the arena
    u32 grow_by = new_size - old_size;
    assert(arena->used + grow_by <= arena->size);

    arena->used += grow_by;

    return ptr;
}

// temporary memory
static Arena_Temp arena_temp_begin(Arena* arena)
{
    Arena_Temp result = {0};

    result.arena = arena;
    result.used = arena->used;

    return result;
}

static void arena_temp_end(Arena_Temp temp)
{
    assert(temp.arena->used > temp.used);
    temp.arena->used = temp.used;
}

//
// string
//

// construct
static String str(char* data, u32 count)
{
    String result = { data, count };
    return result;
}

// format
static String str_format(Arena* arena, char* format, ...)
{
    String result = {0};

    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);

    u32 count_expected = vsnprintf(0, 0, format, args) + 1; // add one for null termination
    va_end(args);

    result.data  = ARENA_PUSH_ARRAY(arena, char, count_expected);
    result.count = vsnprintf(result.data, count_expected, format, args_copy);
    va_end(args_copy);

    assert(result.count + 1 == count_expected);

    result.data[result.count] = 0;
    return result;
}

static String str_copy(Arena* arena, String str)
{
    String result = {0};

    result.count = str.count;
    result.data  = ARENA_PUSH_ARRAY(arena, char, result.count + 1); // + 1 for uncounted null-terminator
    
    for (u32 index = 0; index < result.count; index += 1)
    {
        result.data[index] = str.data[index];
    }

    result.data[result.count] = 0; // null terminator one past last

    return result;
}

// slice
static String str_advance(String* str, u32 count)
{
    String eaten = {0};

    if (str->count >= count)
    {
        eaten.data  = str->data;
        eaten.count = count;

        str->data  += count;
        str->count -= count;
    }

    return eaten;
}

static String str_eat_char(String* str, char char_to_eat)
{
    String eaten = *str;

    while (str->count > 0 && *str->data == char_to_eat)
    {
        str->data++;
        str->count--;
    }
    eaten.count = (u32)(str->data - eaten.data);

    return eaten;
}

static String str_eat_whitespace(String* str)
{
    String eaten = *str;
    for (;;)
    {
        String c = *str;

        if (c.data)
        {
            if (str_is_whitespace(*c.data)) // eats line breaks as well
            {
                c = str_advance(str, 1);
            }
            else if (*c.data == '#') // eat comments
            {
                c = str_advance(str, 1);

                while (*c.data && !str_is_line_break(*c.data))
                {
                    c = str_advance(str, 1);
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    eaten.count = (u32)(str->data - eaten.data);
    return eaten;
}

// compare
static bool str_compare(String a, String b)
{
    if (a.count != b.count) 
    { 
        return false; 
    }

    for (u32 c = 0; c < a.count; ++c)
    {
        if (a.data[c] != b.data[c])
        {
            return false;
        }
    }

    return true;
}

// measure
static u32 str_c_len(char* str)
{
    char* str_copy = str;

    while (*str_copy != 0)
    {
        str_copy++;
    }

    return (u32)(str_copy - str);
}

// classify
static bool str_is_whitespace(char c)
{
    bool result = false;

    if ((c == ' ' ) ||
        (c == '\t') ||
        (c == '\v') ||
        (c == '\f') ||
        str_is_line_break(c))
    {
        result = true;
    }

    return result;
}

static bool str_is_line_break(char c)
{
    bool result = false;

    if ((c == '\n') ||
        (c == '\r'))
    {
        result = true;
    }

    return result;
}

static bool str_is_letter(char c)
{
    bool result = false;

    if ((c >= 'a') && (c <= 'z') ||
        (c >= 'A') && (c <= 'Z'))
    {
        result = true;
    }

    return result;
}

static bool str_is_number(char c)
{
    bool result = false;

    if ((c >= '0') && (c <= '9'))
    {
        result = true;
    }

    return result;
}

static bool str_is_numeric(char c)
{
    bool result = false;

    if (str_is_number(c) || 
        (c == '-') ||
        (c == '.'))
    {
        result = true;
    }

    return result;
}

// string builder
static void strb_append(String_Builder* strb, String str)
{
    u32 new_count = strb->str.count + str.count;
    
    if (strb->str.count == 0)
    {
        strb->str.data = ARENA_PUSH_ARRAY(&strb->arena, char, new_count);
    }
    else
    {
        strb->str.data = ARENA_RESIZE_ARRAY(&strb->arena, char, strb->str.data, strb->str.count, new_count);
    }

    char* src  = str.data;
    char* dest = strb->str.data + strb->str.count;
    while (str.count > 0)
    {
        *dest++ = *src++;

        str.count--;
    }

    strb->str.count = new_count;

    return;
}

static void strb_clear(String_Builder* strb)
{
    arena_reset(&strb->arena);
    strb->str.count = 0;
    return;
}

//
// ring buffer
//

static Ring_Buffer rb_create(u32 capacity)
{
    Ring_Buffer result = {0};

    result.capacity    = capacity;
    result.amount_free = (s64)capacity;
    result.write_index = 0;
    result.read_index  = 0;

    result.data = (u8*)malloc(capacity);

    return result;
}

static void rb_destroy(Ring_Buffer* rb)
{
    if (rb != 0)
    {
        if (rb->data != 0)
        {
            free(rb->data);
            rb->data = 0;
        }

        rb->capacity    = 0;
        rb->amount_free = 0;
        rb->write_index = 0;
        rb->read_index  = 0;
    }

    return;
}

static void rb_clear(Ring_Buffer* rb)
{
    if (rb != 0 && rb->data != 0)
    {
        rb->amount_free = (s64)rb->capacity;
        rb->write_index = 0;
        rb->read_index  = 0;

        for (u32 i = 0; i < rb->capacity; ++i)
        {
            *rb->data++ = 0;
        }
    }

    return;
}

static u32 rb_read(Ring_Buffer* rb, u8* dest, u32 dest_amount)
{
    u32 amount_available = rb->capacity - (u32)rb->amount_free;
    if (amount_available == 0) { return 0; }

    u32 amount_to_read = dest_amount > amount_available ? amount_available : dest_amount;
    u32 amount_left    = amount_to_read;

    if (rb->read_index + amount_to_read >= rb->capacity)
    {
        // will have to wrap back to beginning. Read up to the end of the buffer.

        u32 amount_til_end = rb->capacity - rb->read_index;
        u8* src            = &rb->data[rb->read_index];

        for (u32 i = 0; i < amount_til_end; ++i)
        {
            *dest++ = *src++;
        }
        rb->read_index = 0;
        amount_left -= amount_til_end;
    }

    if (amount_left > 0)
    {
        // if we had to wrap, read the remaining data at the beginning of the ring buffer. 
        // if we don't have to wrap, read all the data at once from the read index.

        u8* src = &rb->data[rb->read_index];

        for (u32 i = 0; i < amount_left; ++i)
        {
            *dest++ = *src++;
        }
        rb->read_index += amount_left;
        amount_left = 0;
    }

    s64 amount_read = (s64)amount_to_read;
    os_win32_atomic_add_s64(&rb->amount_free, amount_read);

    return amount_to_read;
}

static u32 rb_write(Ring_Buffer* rb, u8* src, u32 src_amount)
{
    u32 amount_free = (u32)rb->amount_free;
    if (amount_free == 0) { return 0; }

    u32 amount_to_write = src_amount > amount_free ? amount_free : src_amount;
    u32 amount_left     = amount_to_write;

    if (rb->write_index + amount_left >= rb->capacity)
    {
        // will have to wrap back to beginning. Write up to the end of the buffer.

        u32 amount_til_end = rb->capacity - rb->write_index;
        u8* dest           = &rb->data[rb->write_index];

        for (u32 i = 0; i < amount_til_end; ++i)
        {
            *dest++ = *src++;
        }
        rb->write_index = 0;
        amount_left -= amount_til_end;
    }

    if (amount_left > 0)
    {
        // if we had to wrap, write the remaining data at the beginning of the ring buffer. 
        // if we don't have to wrap, write all the data at once from the write index.

        u8* dest = &rb->data[rb->write_index]; 

        for (u32 i = 0; i < amount_left; ++i)
        {
            *dest++ = *src++;
        }
        rb->write_index += amount_left;
        amount_left = 0;
    }

    s64 amount_written = -1 * ((s64)amount_to_write);
    os_win32_atomic_add_s64(&rb->amount_free, amount_written);

    return amount_to_write;
}

//
// slot pool
//

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

//
// math
//

static f32 lerp(f32 a, f32 b, f32 t)
{
    return (a * (1.0f - t)) + (b * t);
}
