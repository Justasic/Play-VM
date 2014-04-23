/*
 * Copyright (c) 2014, Justin Crawford <Justasic@gmail.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

// Compiled with:
// clang -Wall -Wextra -pedantic -std=c11 -Wshadow -I. -g -D_GNU_SOURCE main2.c -o main2 -pthreads
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#ifndef __STDC_NO_THREADS__
# include <threads.h>
#else
// Use our local hack-around version
# include "threads.h"
#endif

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

// Our registers struct to hold the opcode registers.
typedef struct registers_s
{
	int32_t r0; // General register
	int32_t r1; // General register
	int32_t r2; // General register
	int32_t r3; // Stack pointer
	int32_t r4; // Flags
	int32_t imm;
} registers_t;

// The decoded instruction.
typedef struct instruction_s
{
	uint32_t opcode;
	//unsigned operand; // Technically not needed since registers_t decodes this.
	registers_t *reg;
} instruction_t;

// vm struct to allow for multiple programs
// to run at the same time on the same inter-
// preter. Multiplexing!
typedef struct vm_s
{
        // See the define above
	int32_t regs[NUM_REGS];

        // Our stack -- quite large so we
        // can hold a lot of things in it.
        // Size should be 1 << 16
        unsigned *opstack;

        // Our instruction pointer
        unsigned long ip;

        // The length of the program loaded
        size_t programLength;

        // The program, loaded into a buffer
	int32_t *program;
	
	// Check whether the program is running
	unsigned char running;
	
	// The name of the program
	const char *name;
	size_t nameLen;
	
	// The thread id this vm is running in
	thrd_t thread;
	
	// The next program (if there is one)
	struct vm_s *next;
} vm_t;

// All the mnemonics
enum 
{
        // Basic mnemonics
        OP_UNUSED = 0x000, // Unused -- throw error if used because program is likely corrupt.
        OP_NOP    = 0x001, // No-operation opcode
        OP_ADD    = 0x002, // add two numbers together
        OP_SUB    = 0x003, // subtract two numbers
        OP_MUL    = 0x004, // Multiply two numbers
        OP_DIV    = 0x005, // divide two numbers
        
        // Bitwise operators
        OP_XOR    = 0x006, // bitwise exclusive or
        OP_OR     = 0x007, // bitwise or
        OP_NOT    = 0x008, // bitwise not
        OP_AND    = 0x009, // bitwise and
        OP_SHR    = 0x00A, // bitshift right
        OP_SHL    = 0x00B, // bitshift left

        OP_INC    = 0x00C, // increment register
        OP_DEC    = 0x00D, // decrement register

        // Stack operators
        OP_MOV    = 0x00E, // Move values from register to register
        OP_CMP    = 0x00F, // Compare two registers
        OP_CALL   = 0x010, // Call a function
        OP_RET    = 0x011, // Return from a function call
        OP_PUSH   = 0x012, // Push a value to the stack
        OP_POP    = 0x013, // Pop a value from the stack
        OP_LEA    = 0x014, // Load effective address

        // Jumps
        OP_JMP    = 0x015, // Jump always
        OP_JNZ    = 0x016, // Jump if not zero
        OP_JZ     = 0x017, // Jump if zero
        OP_JEQ    = 0x017, // Jump if equal
        OP_JNE    = 0x018, // Jump if not equal
        OP_JGT    = 0x019, // Jump if greater than
        OP_JLT    = 0x01A, // Jump if less than

        // Program control
        OP_HALT   = 0x01B, // Halt the application
        OP_INT    = 0x01C, // Interrupt -- used for syscalls
	OP_LOADI  = 0x01D, // Load an imm value

        // Debug
        OP_DMP    = 0xA00, // Dump all registers to terminal
        OP_PRNT   = 0xA01 // Dump specific register to the terminal
};

// All flags
enum
{
	FLAG_CARRY    = 0x00000002,
	FLAG_ZERO     = 0x00000004,
	FLAG_EQUAL    = 0x00000008,
	FLAG_OVERFLOW = 0x00000020
};

// This just allocates and prepares our vm_t struct object
vm_t *AllocateVM(void)
{
        vm_t *vm = malloc(sizeof(vm_t));
        memset(vm, 0, sizeof(vm_t));
        vm->opstack = malloc(MAX_STACK);
        memset(vm->opstack, 0, MAX_STACK);
        return vm;
}

// This does the opposite of the above function
void DeallocateVM(vm_t *vm)
{
        free(vm->opstack);
        free(vm->program);
        free(vm);
}

// This deallocates a decoded instruction_t struct.
void DeallocateInstruction(instruction_t *in)
{
	free(in->reg);
	free(in);
}

// This decodes the operands for the instruction
// We pack the operands into a int32_t-sized char
registers_t *DecodeOperand(int32_t operand)
{
	registers_t *reg = malloc(sizeof(registers_t));
	reg->r0  = (operand >> 12) & 0xF ;
	reg->r1  = (operand >>  8) & 0xF ;
	reg->r2  = (operand >>  4) & 0xF ;
	reg->imm = (operand & 0xFF)      ;
	return reg;
}

// Decode an instruction from our program loaded in memory and 
// translate that into a struct.
instruction_t *DecodeInstruction(vm_t *vm)
{
	instruction_t *ins = malloc(sizeof(instruction_t));
	ins->opcode = vm->program[vm->ip++];
	ins->reg = DecodeOperand(vm->program[vm->ip++]);
	return ins;
}

void interpret(vm_t *vm)
{
        // load 2-words (8 bytes) of data and interpret it
        // first word is the instruction and the second is
        // the operands for that instruction. This allows
        // for more registers to be used.
	instruction_t *ins = DecodeInstruction(vm);
        switch(ins->opcode)
        {
		case OP_UNUSED:
			// ignore but print warning
			printf("Unused ovm->ipode encountered... terminating!\n");
			vm->running = 0;
		case OP_NOP:
			// No-Operation
			break;
		case OP_HALT:
			// halt the program
			vm->running = 0;
			break;
		case OP_LOADI:
			// load static value into register
			// 			printf("loadi r%d #%d\n", ins->reg->r0, ins->reg->imm);
			vm->regs[ins->reg->r0] = ins->reg->imm;
			break;
		case OP_ADD:
			// Add values together
			// 			printf("add r%d r%d r%d\n", ins->reg->r0, ins->reg->r1, ins->reg->r2);
			vm->regs[ins->reg->r0] = vm->regs[ins->reg->r1] + vm->regs[ins->reg->r2];
			break;
		case OP_SUB:
			// Subtract values
			vm->regs[ins->reg->r0] = vm->regs[ins->reg->r1] - vm->regs[ins->reg->r2];
			break;
		case OP_DIV:
			// Divide values
			vm->regs[ins->reg->r0] = vm->regs[ins->reg->r1] / vm->regs[ins->reg->r2];
			break;
		case OP_XOR:
			// xor 2 registers
			vm->regs[ins->reg->r0] = vm->regs[ins->reg->r1] ^ vm->regs[ins->reg->r2];
			break;
		case OP_NOT:
			// bitwise not register
			vm->regs[ins->reg->r0] = ~vm->regs[ins->reg->r1];
			break;
		case OP_OR:
			// bitwise or registers
			vm->regs[ins->reg->r0] = vm->regs[ins->reg->r1] | vm->regs[ins->reg->r2];
			break;
		case OP_AND:
			// bitwise and registers
			vm->regs[ins->reg->r0] = (vm->regs[ins->reg->r1] & vm->regs[ins->reg->r2]);
			break;
		case OP_SHL:
			// bitshift left
			vm->regs[ins->reg->r0] = vm->regs[ins->reg->r1] << vm->regs[ins->reg->r2];
			break;
		case OP_SHR:
			// bitshift right
			vm->regs[ins->reg->r0] = vm->regs[ins->reg->r1] >> vm->regs[ins->reg->r2];
			break;
		case OP_INC:
			// increment register
			vm->regs[ins->reg->r0]++;
			break;
		case OP_DEC:
			// decrement register
			vm->regs[ins->reg->r0]--;
			break;
		case OP_CMP:
			// compare 2 registers together
			vm->regs[ins->reg->r0] = (vm->regs[ins->reg->r1] == vm->regs[ins->reg->r2]);
			break;
		case OP_MOV:
			// move values from register to register
			// (and register to stack when stack implemented)
			vm->regs[ins->reg->r1] = vm->regs[ins->reg->r0];
			break;
		case OP_CALL:
			// Call a section of code.
			// This is basically a push + jmp call in one.
			vm->opstack[vm->regs[3]] = vm->ip+1; // Push program position + 1 to stack -- the +1 is to move past the call instruction so we don't do an infinite loop
			vm->regs[3]++;             // Increment stack pointer
			vm->ip = vm->regs[ins->reg->r0];       // set program position
			//printf("Saved location: %d -- jumping to location %d\n", vm->opstack[vm->regs[3]-1], vm->ip);
			break;                 // return, next iteration by CPU will be at the new position
// 		case OP_CALLF:
// 			// Same as Call but using ins->reg->imm instead
// 			vm->opstack[vm->regs[3]] = vm->ip+1;
// 			vm->regs[3]++;
// 			vm->ip = ins->reg->imm;
// 			break;
		case OP_RET:
			// This the opposite of call.
			vm->regs[3]--;             // Decrement stack pointer
			vm->ip = vm->opstack[vm->regs[3]]; // Get the previous run position from stack
			//printf("Returning back to %d\n", vm->ip);
			break;                 // return, next iteration by CPU will be at new position
		case OP_PUSH:
			// Push value onto stack
			// we'll treat register 4 as the stack pointer.
			vm->opstack[vm->regs[3]] = vm->regs[ins->reg->r0];
			vm->regs[3]++;
			break;
		case OP_POP:
			// pop value from stack
			vm->regs[ins->reg->r0] = vm->opstack[vm->regs[3]];
			vm->regs[3]--;
			break;
		case OP_JMP:
			vm->ip = vm->regs[ins->reg->r0];
			break;
		case OP_JNZ:
		case OP_JZ:
			printf("Ignoring unimplemented opcode %d\n", ins->opcode);
			break;
			
			// Extended opcode which will later be removed.
		case OP_PRNT:
			printf("r%d: %d\n", ins->reg->r0, vm->regs[ins->reg->r0]);
			break;
		case OP_DMP:
			printf("Registers:\nr0: %d\nr1: %d\nr2: %d\nr3: %d\n", vm->regs[0], vm->regs[1], vm->regs[2], vm->regs[3]);
			break;
                default:
                        printf("Unknown opcode 0x%x!\n", ins->opcode);
        };
	
	// Deallocate our instruction we used.
	DeallocateInstruction(ins);
}

// A mutex to make sure we don't cause any issues when
// changing the linked list.
mtx_t listmutex;

// Our linked list
vm_t *first = NULL, *prev = NULL;

// This is the thread function used to
// decode the program
void DecodeThread(void *ptr)
{
	vm_t *me = (vm_t*)ptr;
	// While the vm is still running
	// decode each instruction and 
	// run it.
	while(me->running)
		interpret(me);

	// Modify the linked list so we can remove ourselves
	// from the list.
	mtx_lock(&listmutex);
	
	// Iterate through the linked list, we're going to remove ourselves
	// from the list so we can deallocate.
	for (vm_t *vm = first, *prevvm = NULL; vm; prevvm = vm, vm = vm->next)
	{
		// If we've found ourselves
		if (vm == me)
		{
			// if we're first, set the first global var to
			// the next vm in the list
			if (me == first)
				first = me->next;
			else
				// Tack together the previous vm to the next.
				prevvm->next = me->next;
		}
	}
	
	// Make sure our pointer is unusable
	me->next = NULL;

	// Unlock our mutex.
	mtx_unlock(&listmutex);
	
	// Deallocate ourselves
	DeallocateVM(me);
}

int main(int argc, char **argv)
{
        for (int i = 0; i < argc; ++i)
	{
		printf("param[%d]: %s\n", i, argv[i]);
	}
	
	if (argc < 2 || (argc > 2 && (!strcasecmp(argv[1], "--help") || !strcasecmp(argv[1], "-h"))))
	{
		fprintf(stderr, "Basic virtual machine interpreter written by Justin Crawford\n\n");
		fprintf(stderr, "USAGE: %s [options] application ...\n\n", argv[0]);
		fprintf(stderr, "OPTIONS:\n");
		fprintf(stderr, "-d, --dump         Dump the loaded program as hex to stdout\n");
		fprintf(stderr, "-h, --help         Print this message.\n");
		return 1;
	}
	
	for (int i = 0; i < argc; ++i)
	{
		char *program = argv[i];
		size_t len = strlen(program);
		
		if (program[0] == '-') // Skip programs which start with '-' in their names.. might be an option
			continue;
		if (program[0] == '-' && program[1] == '-')
			continue;
		
		vm_t *vm = AllocateVM();
		
		// Set our name and shit
		vm->name = program;
		vm->nameLen = len;
		// We're running
		vm->running = 1;
		
		// Open a file as read-binary
		FILE *f = fopen(vm->name, "rb");
		
		if(!f)
		{
			fprintf(stderr, "Failed to open %s. Skipping.\n", vm->name);
			DeallocateVM(vm);
			continue;
		}
		
		// Get program length
		fseek(f, 0, SEEK_END);
		vm->programLength = ftell(f);
		rewind(f);
		
		// Make sure our program is empty
		vm->program = malloc(vm->programLength+1);
		// Make sure the whole buffer is null before we start
		memset(vm->program, 0, vm->programLength+1);
		
		// Read each byte into our buffer
		size_t ret = fread(vm->program, 1, vm->programLength, f);
		
		// Close our file descriptor
		fclose(f);
		
		// Make sure the program has a valid length and isn't null.
		if (ret != vm->programLength || !ret)
		{
			fprintf(stderr, "Failed to read program: invalid length!\n");
			DeallocateVM(vm);
			continue;
		}
		
		// Check if this is the first element in the linked list.
		if (!first)
			first = vm;
		// Set the previous vm so when we allocate our next
		// vm, we can add it to the linked list
		prev = vm;
	}
	
	// Initialize the mutex
	mtx_init(&listmutex, mtx_plain);
	
	// Lock our mutex in case our programs are super small
	// and we haven't even finished loading our program(s)
	mtx_lock(&listmutex);
	
	// Iterate the vms and start running them
	for (vm_t *vm = first; vm; vm = vm->next)
	{
		int ret = thrd_create(&vm->thread, DecodeThread, vm);
		if (ret != thrd_success)
			fprintf(stderr, "Failed to start a new program thread for program %s!\n", vm->name);
	}
	
	// Unlock and let the threads do their thing.
	mtx_unlock(&listmutex);
	
        return 0;
}
