#ifndef VM_H
#define VM_H

#include <stdint.h>
#include "value.h"
#include "chunk.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stack_top;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void init_vm(VM* vm);
void free_vm(VM* vm);
InterpretResult interpret(VM* vm, const char* source);
void push(VM* vm, Value value);
Value pop(VM* vm);

bool is_falsey(Value value);
bool values_equal(Value a, Value b);

#endif
