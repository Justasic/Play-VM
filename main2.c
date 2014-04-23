#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


// Our registers
//
// r0 - general register
// r1 - general register
// r2 - general register
// r3 - stack pointer register
// r4 - flags register
#define NUM_REGS 5

// Our max stack size
#define MAX_STACK (1 << 16)

// vm struct to allow for multiple programs
// to run at the same time on the same inter-
// preter. Multiplexing!
typedef struct vm_s
{
        // See the define above
        unsigned regs[NUM_REGS];

        // Our stack -- quite large so we
        // can hold a lot of things in it.
        // Size should be 1 << 16
        unsigned *opstack;

        // Our instruction pointer
        unsigned long ip;

        // The length of the program loaded
        size_t programLength;

        // The program, loaded into a buffer
        unsigned char *program;
} vm_t;

// All the mnemonics
enum 
{
        // Basic mnemonics
        OP_UNUSED = 0x000; // Unused -- throw error if used because program is likely corrupt.
        OP_NOP    = 0x001; // No-operation opcode
        OP_ADD    = 0x002; // add two numbers together
        OP_SUB    = 0x003; // subtract two numbers
        OP_MUL    = 0x004; // Multiply two numbers
        OP_DIV    = 0x005; // divide two numbers
        
        // Bitwise operators
        OP_XOR    = 0x006; // bitwise exclusive or
        OP_OR     = 0x007; // bitwise or
        OP_NOT    = 0x008; // bitwise not
        OP_AND    = 0x009; // bitwise and
        OP_SHR    = 0x00A; // bitshift right
        OP_SHL    = 0x00B; // bitshift left

        OP_INC    = 0x00C; // increment register
        OP_DEC    = 0x00D; // decrement register

        // Stack operators
        OP_MOV    = 0x00E; // Move values from register to register
        OP_CMP    = 0x00F; // Compare two registers
        OP_CALL   = 0x010; // Call a function
        OP_RET    = 0x011; // Return from a function call
        OP_PUSH   = 0x012; // Push a value to the stack
        OP_POP    = 0x013; // Pop a value from the stack
        OP_LEA    = 0x014; // Load effective address

        // Jumps
        OP_JMP    = 0x015; // Jump always
        OP_JNZ    = 0x016; // Jump if not zero
        OP_JZ     = 0x017; // Jump if zero
        OP_JEQ    = 0x017; // Jump if equal
        OP_JNE    = 0x018; // Jump if not equal
        OP_JGT    = 0x019; // Jump if greater than
        OP_JLT    = 0x01A; // Jump if less than

        // Program control
        OP_HALT   = 0x01B; // Halt the application
        OP_INT    = 0x01C; // Interrupt -- used for syscalls

        // Debug
        OP_DMP    = 0xA00; // Dump all registers to terminal
        OP_PRNT   = 0xA01; // Dump specific register to the terminal
};

vm_t *AllocateVM(void)
{
        vm_t *vm = malloc(sizeof(vm_t));
        memset(vm, 0, sizeof(vm_t));
        vm->opstack = malloc(MAX_STACK);
        memset(vm->opstack, 0, MAX_STACK);
        return vm;
}

void DeallocateVM(vm_t *vm)
{
        free(vm->opstack);
        free(vm->program);
        free(vm);
}

unsigned readopcode(vm_t *vm)
{
        if (vm->ip > vm->programLength)
        {
                printf("Error: Instruction Pointer tried to access outside bounds of the program. Terminating\n");
                return 0;
        }

        return vm->program[vm->ip];
}

unsigned read

void interpret(vm_t *vm)
{
        // load 2-words (8 bytes) of data and interpret it
        // first word is the instruction and the second is
        // the operands for that instruction. This allows
        // for more registers to be used.
        switch(readopcode(vm))
        {
                default:
                        printf("Unknown opcode!\n");
        };
}

int main(int argc, char **argv)
{
        
        return 0;
}
