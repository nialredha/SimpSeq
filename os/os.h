#ifndef BASE_OS_H
#define BASE_OS_H

static String os_read_entire_file (String filename);
static bool   os_write_entire_file(String filename, String content);

static void os_sleep_ms(u32 milliseconds);

s32 main2(s32 arg_count, char** args);

#endif // BASE_OS_H
