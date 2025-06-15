#ifndef GC_H
#define GC_H

#include "bloat.h"
#include "vm.h"

void gc_init(VM* vm);
void gc_collect(VM* vm);
void* gc_alloc(VM* vm, size_t size);
void gc_free(VM* vm, void* ptr);

#endif
