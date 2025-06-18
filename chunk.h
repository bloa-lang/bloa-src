#ifndef CHUNK_H
#define CHUNK_H

#include "bloat.h"
#include "value.h"
#include "memory.h"

#define MAX_CONSTANTS 65536

typedef struct {
    uint8_t* code;
    int* lines;
    int count;
    int capacity;
    Value* constants;
    int constant_count;
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void write_chunk(Chunk* chunk, uint8_t byte, int line);
int add_constant(Chunk* chunk, Value value);

#endif
