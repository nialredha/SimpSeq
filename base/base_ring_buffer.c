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

