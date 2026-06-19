#ifndef CORE_H
#define CORE_H

#include <assert.h>

#include <math.h>
#include <malloc.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

//
// arena
//

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
static void arena_alloc  (Arena* arena, u32 size);
static void arena_realloc(Arena* arena, u32 new_size);
static void arena_free   (Arena* arena);

// update
static void  arena_reset (Arena* arena);
static void* arena_push  (Arena* arena, u32 size);
static void* arena_resize(Arena* arena, void* ptr, u32 old_size, u32 new_size);

// temporary memory
static Arena_Temp arena_temp_begin(Arena* arena);
static void       arena_temp_end  (Arena_Temp temp);


//
// string
//

typedef struct
{
    char* data;
    u32   count;
} String;

typedef struct
{
    Arena  arena;
    String str;
} String_Builder;

#define STR_LIT(s) str((char*)(s), sizeof(s) - 1)
#define STR_C(s)   str((char*)(s), str_c_len(s)) 

// construct
static String str(char* data, u32 count);

// format
static String str_format(Arena* arena, char* format, ...);

// copy
static String str_copy(Arena* arena, String str);

// slice
static String str_advance(String* str, u32 count);
static String str_eat_char(String* str, char char_to_eat);
static String str_eat_whitespace(String* str);

// compare
static bool str_compare(String a, String b);

// measure
static u32 str_c_len(char* data);

// classify
static bool str_is_whitespace(char c);
static bool str_is_line_break(char c);
static bool str_is_letter(char c);
static bool str_is_number(char c);
static bool str_is_numeric(char c);

// string builder
static void strb_append(String_Builder* strb, String str);
static void strb_clear (String_Builder* strb);

//
// ring buffer
//

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

//
// slot pool
//

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

//
// math
//

static f32 lerp(f32 a, f32 b, f32 t);

#endif // CORE_H
