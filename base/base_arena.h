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
} Arena_Temp;

#define ARENA_PUSH_STRUCT(arena, type)       (type*)arena_push(arena, sizeof(type))
#define ARENA_PUSH_ARRAY(arena, type, count) (type*)arena_push(arena, sizeof(type) * (count))

#define ARENA_RESIZE_ARRAY(arena, type, ptr, old_count, new_count) (type*)arena_resize(arena, (void*)(ptr), sizeof(type)*old_count, sizeof(type)*new_count)

// allocate/deallocate
void arena_alloc  (Arena* arena, u32 size);
void arena_realloc(Arena* arena, u32 new_size);
void arena_free   (Arena* arena);

// update
void  arena_reset (Arena* arena);
void* arena_push  (Arena* arena, u32 size);
void* arena_resize(Arena* arena, void* ptr, u32 old_size, u32 new_size);

// temporary memory
Arena_Temp arena_temp_begin(Arena* arena);
void       arena_temp_end  (Arena_Temp temp);

#endif // BASE_ARENA_H
