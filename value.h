#ifndef VALUE_H
#define VALUE_H

#include <stdbool.h>

typedef enum {
    TYPE_NIL,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool bool_val;
        int int_val;
        double float_val;
        const char* str_val;
    } data;
} Value;

#define NIL_VAL ((Value){ TYPE_NIL, { .int_val = 0 } })
#define BOOL_VAL(v) ((Value){ TYPE_BOOL, { .bool_val = v } })
#define INT_VAL(v) ((Value){ TYPE_INT, { .int_val = v } })
#define NUMBER_VAL(v) ((Value){ TYPE_FLOAT, { .float_val = v } })
#define STRING_VAL(s) ((Value){ TYPE_STRING, { .str_val = s } })

#define IS_BOOL(v) ((v).type == TYPE_BOOL)
#define IS_NIL(v) ((v).type == TYPE_NIL)
#define IS_INT(v) ((v).type == TYPE_INT)
#define IS_FLOAT(v) ((v).type == TYPE_FLOAT)
#define IS_STRING(v) ((v).type == TYPE_STRING)
#define IS_NUMBER(v) (IS_INT(v) || IS_FLOAT(v))

#define AS_BOOL(v) ((v).data.bool_val)
#define AS_INT(v) ((v).data.int_val)
#define AS_FLOAT(v) ((v).data.float_val)
#define AS_STRING(v) ((v).data.str_val)

void print_value(Value value);

#endif
