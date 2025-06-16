#include <stdio.h>
#include "value.h"

void print_value(Value value) {
    switch (value.type) {
        case TYPE_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case TYPE_NIL:
            printf("nil");
            break;
        case TYPE_INT:
            printf("%d", AS_INT(value));
            break;
        case TYPE_FLOAT:
            printf("%g", AS_FLOAT(value));
            break;
        case TYPE_STRING:
            printf("\"%s\"", AS_STRING(value));
            break;
        default:
            printf("unknown");
            break;
    }
}
