#include "bloat.h"
#include "chunk.h"

#include <stdint.h>
#include <stdlib.h>

void init_chunk(Chunk* chunk) {
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->constants = NULL;
    chunk->constant_count = 0;
}

void free_chunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    FREE_ARRAY(Value, chunk->constants, chunk->capacity);
    init_chunk(chunk);
}

void write_chunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, 
                                old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines,
                                old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int add_constant(Chunk* chunk, Value value) {
    if (chunk->constant_count == MAX_CONSTANTS) {
        fprintf(stderr, "Too many constants in one chunk.\n");
        exit(1);
    }
    chunk->constants[chunk->constant_count] = value;
    return chunk->constant_count++;
}
