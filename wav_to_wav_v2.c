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

    String filename     = STR_C(argv[1]);
    String file         = read_entire_file(filename);
    Chunk_Node_V2* root = wav_chunks_from_file_v2(&chunk_arena, file);

    Arena file_arena   = {0};
    arena_alloc(&file_arena, 1024*1024);
    String file_clone = wav_file_from_chunks_v2(&file_arena, root);

    write_entire_file(STR_LIT("file_clone_v2.wav"), file_clone);

    return 0;
}
