#include "bloat.h"
#include "gc.h"

static void mark_object(VM* vm, Object* object) {
    if (object == NULL || object->marked) return;
    object->marked = true;
}

static void mark_value(VM* vm, Value value) {
    if (IS_OBJ(value)) mark_object(vm, AS_OBJ(value));
}

static void mark_roots(VM* vm) {
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
        mark_value(vm, *slot);
    }
}

static void trace_references(VM* vm) {
    Object* object = vm->objects;
    while (object != NULL) {
        mark_object(vm, object);
        object = object->next;
    }
}

static void sweep(VM* vm) {
    Object** object = &vm->objects;
    while (*object != NULL) {
        if (!(*object)->marked) {
            Object* unreached = *object;
            *object = unreached->next;
            free(unreached);
            vm->bytes_allocated -= sizeof(Object);
        } else {
            (*object)->marked = false;
            object = &(*object)->next;
        }
    }
}

void gc_collect(VM* vm) {
    mark_roots(vm);
    trace_references(vm);
    sweep(vm);

    vm->next_gc = vm->bytes_allocated * 2;
}

void* gc_alloc(VM* vm, size_t size) {
    if (vm->bytes_allocated + size > vm->next_gc) {
        gc_collect(vm);
    }

    void* memory = malloc(size);
    if (memory == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }

    vm->bytes_allocated += size;
    return memory;
}

void gc_free(VM* vm, void* ptr) {
    free(ptr);
    vm->bytes_allocated -= sizeof(Object);
}
