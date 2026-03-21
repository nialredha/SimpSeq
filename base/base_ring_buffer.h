#ifndef BASE_RING_BUFFER_H
#define BASE_RING_BUFFER_H

typedef struct 
{
    u32 capacity;
    u32 write_index;
    u32 read_index;

    s64 amount_free;

    u8* data;
} Ring_Buffer;

static Ring_Buffer rb_create (u32 capacity);
static void        rb_destroy(Ring_Buffer* rb);

static void rb_clear(Ring_Buffer* rb);

static u32 rb_read (Ring_Buffer* rb, u8* dest, u32 dest_amount);
static u32 rb_write(Ring_Buffer* rb, u8* src, u32 src_amount);

#endif // BASE_RING_BUFFER_H
