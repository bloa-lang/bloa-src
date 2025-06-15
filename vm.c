#include "bloat.h"
#include "compiler.h"
#include "vm.h"

static InterpretResult run() {
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants[READ_BYTE()])
    #define BINARY_OP(op) \
        do { \
            double b = pop(); \
            double a = pop(); \
            push(a op b); \
        } while (false)

    for (;;) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(>); break;
            case OP_LESS:     BINARY_OP(<); break;
            case OP_ADD:      BINARY_OP(+); break;
            case OP_SUBTRACT:  BINARY_OP(-); break;
            case OP_MULTIPLY:  BINARY_OP(*); break;
            case OP_DIVIDE:    BINARY_OP(/); break;
            case OP_NOT:
                push(BOOL_VAL(is_falsey(pop())));
                break;
            case OP_NEGATE: {
                Value value = pop();
                if (!IS_NUMBER(value)) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(value)));
                break;
            }
            case OP_PRINT: {
                print_value(pop());
                printf("\n");
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef BINARY_OP
}

void init_vm(VM* vm) {
    vm->stack_top = vm->stack;
}

void free_vm(VM* vm) {
    // Nothing to free yet
}

InterpretResult interpret(VM* vm, const char* source) {
    Chunk chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm->chunk = &chunk;
    vm->ip = vm->chunk->code;

    InterpretResult result = run();

    free_chunk(&chunk);
    return result;
}

void push(VM* vm, Value value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

Value pop(VM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

bool is_falsey(Value value) {
    return (value.type == TYPE_NIL) || 
           (value.type == TYPE_BOOL && !value.data.bool_val);
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case TYPE_BOOL:   return a.data.bool_val == b.data.bool_val;
        case TYPE_NIL:    return true;
        case TYPE_INT:    return a.data.int_val == b.data.int_val;
        case TYPE_FLOAT:  return a.data.float_val == b.data.float_val;
        case TYPE_STRING: return strcmp(a.data.str_val, b.data.str_val) == 0;
        default:          return false;
    }
}
