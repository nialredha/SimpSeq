static String read_entire_file(String filename)
{
    String result = {0};

    FILE* file = fopen(filename.data, "rb");

    if (file)
    {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        fseek(file, 0, SEEK_SET);

        result.data = (char*)malloc(size);
        if (result.data)
        {
            size_t result_size = fread(result.data, sizeof(*result.data), size, file);
            if (result_size == size)
            {
                result.count = (u32)size;
            }
            else
            {
                free(result.data);
                result.data = 0;
                result.count = 0;
            }
        }

        fclose(file);
    }

    return result;
}

static bool write_entire_file(String filename, String content)
{
    bool success = true;

    FILE* file = fopen(filename.data, "wb");

    if (file)
    {
        u32 result = (u32)fwrite(content.data, 1, content.count, file);

        if (result != content.count)
        {
            result = false;
        }

        fclose(file);
    }

    return success;
}
