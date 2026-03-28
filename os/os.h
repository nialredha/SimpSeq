#ifndef BASE_OS_H
#define BASE_OS_H

typedef struct
{
    u64 size;
    u64 time_modified;
    u64 time_created;
} OS_File_Properties;

static String os_read_entire_file (String filename);
static bool   os_write_entire_file(String filename, String content);

static OS_File_Properties os_get_file_properties(String filepath);

static String os_read_entire_file (String filepath);
static bool   os_write_entire_file(String filename, String contents);

static void os_free_file_contents(String* contents);

static u64  os_now_us  (void);
static void os_sleep_ms(u32 milliseconds);

s32 entry_point(s32 arg_count, char** args);

#endif // BASE_OS_H
