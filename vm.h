#ifndef VM_H
#define VM_H

#include "bloat.h"
#include "chunk.h"
#include "value.h"

#include <stdint.h>
#include <stddef.h>

#define STACK_MAX 256
#define MAX_CONSTANTS 65536

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stack_top;
    Object* objects;
    size_t bytes_allocated;
    size_t next_gc;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

void init_vm(VM* vm);
void free_vm(VM* vm);
InterpretResult interpret(VM* vm, const char* source);
void push(VM* vm, Value value);
Value pop(VM* vm);

bool is_falsey(Value value);
bool values_equal(Value a, Value b);

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void collect_garbage(VM* vm);
void mark_object(VM* vm, Object* object);
void mark_value(VM* vm, Value value);

#define IS_OBJ(value)     ((value).type == VAL_OBJ)
#define AS_OBJ(value)     ((value).as.obj)

#endif
