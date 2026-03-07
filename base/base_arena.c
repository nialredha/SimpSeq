// allocate/deallocate
void arena_alloc(Arena* arena, u32 size)
{
    arena->base = (u8*)malloc(size);
    if (arena->base != 0)
    {
        arena->size = size;
        arena->used = 0;
    }
}

void arena_realloc(Arena* arena, u32 new_size)
{
    arena->base = (u8*)realloc(arena->base, new_size);
    if (arena->base != 0)
    {
        arena->size = new_size;
    }
}

void arena_free(Arena* arena)
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
void arena_reset(Arena* arena)
{
    arena->used = 0;
}

void* arena_push(Arena* arena, u32 size)
{
    assert(arena->used + size <= arena->size);

    void* result = arena->base + arena->used;
    arena->used += size;

    return result;
}

void* arena_resize(Arena* arena, void* ptr, u32 old_size, u32 new_size)
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
Arena_Temp arena_temp_begin(Arena* arena)
{
    Arena_Temp result = {0};

    result.arena = arena;
    result.used = arena->used;

    return result;
}

void arena_temp_end(Arena_Temp temp)
{
    assert(temp.arena->used > temp.used);
    temp.arena->used = temp.used;
}
