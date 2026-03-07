#ifndef BASE_STRING_H
#define BASE_STRING_H

typedef struct
{
    char*  data;
    u32 count;
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
u32 str_c_len(char* data);

// classify
bool str_is_whitespace(char c);
bool str_is_line_break(char c);
bool str_is_letter(char c);
bool str_is_number(char c);
bool str_is_numeric(char c);

// string builder
String_Builder strb_new(u32 size);
void strb_append(String_Builder* strb, String str);;

#endif // BASE_STRING_H
