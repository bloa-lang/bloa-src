#ifndef BLOAT_H
#define BLOAT_H

#include "value.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VARS 256
#define MAX_STACK 1024
#define INITIAL_GC_THRESHOLD 1024

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

#endif
