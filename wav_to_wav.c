#include "base_core.h"
#include "base_arena.h"
#include "base_string.h"
#include "base_io.h"
#include "wav.h"

#include "base_arena.c"
#include "base_string.c"
#include "base_io.c"
#include "wav.c"

int main(int args, char** argv)
{
    if (args != 2)
    {
        printf("Not enough arguments!\n Example usage: .\\%s test.wav\n", argv[0]);
        return 1;
    }

    Arena chunk_arena = {0};
    arena_alloc(&chunk_arena, 4096);

    String filename   = STR_C(argv[1]);
    String file       = read_entire_file(filename);
    Chunk_Node* first = wav_chunks_from_file(&chunk_arena, file);

    Arena file_arena   = {0};
    arena_alloc(&file_arena, 1024*1024);
    String file_clone = wav_file_from_chunks(&file_arena, first);

    write_entire_file(STR_LIT("file_clone.wav"), file_clone);

    return 0;
}
