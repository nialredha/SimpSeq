#ifndef BASE_ARENA_H
#define BASE_ARENA_H

typedef struct
{
    u8*  base;
    u32  size;
    u32  used;
} Arena;

typedef struct
{
    Arena* arena;
    u32 used;
} Temp_Arena;

#define ARENA_PUSH_STRUCT(arena, type)       (type*)arena_push(arena, sizeof(type))
#define ARENA_PUSH_ARRAY(arena, type, count) (type*)arena_push(arena, sizeof(type) * (count))

// allocate/deallocate
void arena_alloc (Arena* arena, u32 size);
void arena_free  (Arena* arena);

// update
void arena_reset(Arena* arena);
void* arena_push(Arena* arena, u32 size);

// temporary memory
Temp_Arena temp_begin(Arena* arena);
void temp_end(Temp_Arena temp_arena);

#endif // BASE_ARENA_H
