// construct
static String str(char* data, u32 count)
{
    String result = { data, count };
    return result;
}

// format
static String str_format(Arena* arena, char* format, ...)
{
    String result = {0};

    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);

    u32 count_expected = vsnprintf(0, 0, format, args) + 1; // add one for null termination
    va_end(args);

    result.data  = ARENA_PUSH_ARRAY(arena, char, count_expected);
    result.count = vsnprintf(result.data, count_expected, format, args_copy);
    va_end(args_copy);

    assert(result.count + 1 == count_expected);

    result.data[result.count] = 0;
    return result;
}

static String str_copy(Arena* arena, String str)
{
    String result = {0};

    result.count = str.count;
    result.data  = ARENA_PUSH_ARRAY(arena, char, result.count + 1); // + 1 for uncounted null-terminator
    
    for (u32 index = 0; index < result.count; index += 1)
    {
        result.data[index] = str.data[index];
    }

    result.data[result.count] = 0; // null terminator one past last

    return result;
}

// slice
static String str_advance(String* str, u32 count)
{
    String eaten = {0};

    if (str->count > count)
    {
        eaten.data  = str->data;
        eaten.count = count;

        str->data  += count;
        str->count -= count;
    }
    else
    {
        str->data = '\0';
        str->count = 1;

        eaten.data  = str->data;
        eaten.count = str->count;
    }

    return eaten;
}

static String str_eat_char(String* str, char char_to_eat)
{
    String eaten = *str;

    while (str->count > 0 && *str->data == char_to_eat)
    {
        str->data++;
        str->count--;
    }
    eaten.count = (u32)(str->data - eaten.data);

    return eaten;
}

static String str_eat_whitespace(String* str)
{
    String eaten = *str;
    for (;;)
    {
        String c = *str;

        if (c.data)
        {
            if (str_is_whitespace(*c.data)) // eats line breaks as well
            {
                c = str_advance(str, 1);
            }
            else if (*c.data == '#') // eat comments
            {
                c = str_advance(str, 1);

                while (*c.data && !str_is_line_break(*c.data))
                {
                    c = str_advance(str, 1);
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    eaten.count = (u32)(str->data - eaten.data);
    return eaten;
}

// compare
static bool str_compare(String a, String b)
{
    if (a.count != b.count) 
    { 
        return false; 
    }

    for (u32 c = 0; c < a.count; ++c)
    {
        if (a.data[c] != b.data[c])
        {
            return false;
        }
    }

    return true;
}

// measure
u32 str_c_len(char* str)
{
    char* str_copy = str;

    while (*str_copy != 0)
    {
        str_copy++;
    }

    return (u32)(str_copy - str);
}

// classify
bool str_is_whitespace(char c)
{
    bool result = false;

    if ((c == ' ' ) ||
        (c == '\t') ||
        (c == '\v') ||
        (c == '\f') ||
        str_is_line_break(c))
    {
        result = true;
    }

    return result;
}

bool str_is_line_break(char c)
{
    bool result = false;

    if ((c == '\n') ||
        (c == '\r'))
    {
        result = true;
    }

    return result;
}

bool str_is_letter(char c)
{
    bool result = false;

    if ((c >= 'a') && (c <= 'z') ||
        (c >= 'A') && (c <= 'Z'))
    {
        result = true;
    }

    return result;
}

bool str_is_number(char c)
{
    bool result = false;

    if ((c >= '0') && (c <= '9'))
    {
        result = true;
    }

    return result;
}

bool str_is_numeric(char c)
{
    bool result = false;

    if (str_is_number(c) || 
        (c == '-') ||
        (c == '.'))
    {
        result = true;
    }

    return result;
}

// print
void str_print(String str)
{
    if (str.data && str.count > 0)
    {
        for (u32 char_index = 0; 
             char_index < str.count; 
             ++char_index)
        {
            printf("%c", str.data[char_index]);
        }
    }
}

