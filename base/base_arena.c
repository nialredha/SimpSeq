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

// temporary memory
Temp_Arena temp_begin(Arena* arena)
{
    Temp_Arena result = {0};

    result.arena = arena;
    result.used = arena->used;

    return result;
}

void temp_end(Temp_Arena temp_arena)
{
    assert(temp_arena.arena->used > temp_arena.used);
    temp_arena.arena->used = temp_arena.used;
}
