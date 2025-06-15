#ifndef BLOAT_H
#define BLOAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VARS 256
#define MAX_STACK 1024
#define INITIAL_GC_THRESHOLD 1024
#define MAX_CONSTANTS 256

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)
#define GROW_ARRAY(type, ptr, old, new) (type*)realloc(ptr, sizeof(type) * (new))
#define FREE_ARRAY(type, ptr, old) free(ptr)

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_NIL
} ValueType;

typedef struct {
    ValueType type;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
    } data;
} Value;

#define NIL_VAL ((Value){TYPE_NIL, {.int_val = 0}})
#define BOOL_VAL(b) ((Value){TYPE_BOOL, {.bool_val = b}})
#define INT_VAL(n) ((Value){TYPE_INT, {.int_val = n}})
#define FLOAT_VAL(n) ((Value){TYPE_FLOAT, {.float_val = n}})
#define NUMBER_VAL(n) FLOAT_VAL(n)
#define IS_OBJ(value) ((value).type == TYPE_STRING)
#define AS_OBJ(value) ((Object*)(value).data.str_val)

typedef struct Object {
    struct Object* next;
    bool marked;
    Value value;
} Object;

typedef struct {
    char* name;
    Value value;
} Variable;

typedef struct {
    Variable variables[MAX_VARS];
    int count;
} Environment;

typedef struct {
    uint8_t* code;
    int* lines;
    int count;
    int capacity;
    Value* constants;
    int constant_count;
} Chunk;

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[MAX_STACK];
    Value* stack_top;
    Environment* env;
    Object* objects;
    size_t bytes_allocated;
    size_t next_gc;
} VM;

#endif
