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
// clang -Wall -Wextra -pedantic -std=c11 -Wshadow -I. -g main2.c -o main2 -pthreads

// Needed for the bullshit license issues - Justasic
#define _POSIX_C_SOURCE 200809L 
#define __USE_XOPEN2K8 1
#define __USE_XOPEN2K 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h> // fuck this header
#include <stdint.h>
#include <limits.h>
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

// Some flag functions
#define SETFLAGS(var, flags)   (var |= (flags))
#define UNSETFLAGS(var, flags) (var &= ~(flags))

// Other macros
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

// Our registers struct to hold the opcode registers.
typedef struct registers_s
{
	int32_t r0; // General register
	int32_t r1; // General register
	int32_t r2; // General register
	int32_t r3; // Stack pointer
	int32_t r4; // Flags
	int32_t imm; // immediate value
} registers_t;

// The decoded instruction.
typedef struct instruction_s
{
	// The opcode that was decoded
	uint32_t opcode;
	//unsigned operand; // Technically not needed since registers_t decodes this.
	// The type of the operands (whether it's a register or an immediate constant value)
	uint8_t type;
	// operands (decoded)
	registers_t *reg;
} instruction_t;

// This is just a struct to use in the struct below
// it corrects the instruction pointer
typedef struct program_s
{
	int32_t opcode;
	int32_t operands;
} program_t;

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
	program_t **program;
	
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
        OP_JS     = 0x018, // Jump if sign
	OP_JNS    = 0x019, // Jump if not sign
        OP_JGT    = 0x01A, // Jump if greater than
        OP_JLT    = 0x01B, // Jump if less than
	OP_JPE    = 0x01C, // Jump if parity even
	OP_JPO    = 0x01D, // Jump if parity odd

        // Program control
        OP_HALT   = 0x01E, // Halt the application
        OP_INT    = 0x01F, // Interrupt -- used for syscalls
	OP_LOADI  = 0x020, // Load an imm value
	OP_PUSHF  = 0x021, // Push flags to stack
	OP_POPF   = 0x022, // Pop flags from stack

        // Debug
        OP_DMP    = 0xA00, // Dump all registers to terminal
        OP_PRNT   = 0xA01 // Dump specific register to the terminal
};

// All flags
enum
{
	FLAG_CARRY    = (1 << 0), // If an arithmatic carry operation occured
	FLAG_ZERO     = (1 << 1), // If the operation resulted in a zero result
	FLAG_OVERFLOW = (1 << 2), // If the operation overflowed the integer
	FLAG_SIGN     = (1 << 3), // If the operation used a signed integer that is negative
	FLAG_PARITY   = (1 << 4)  // see http://en.wikipedia.org/wiki/Parity_flag
};

// used to tell whether the opcode is to use
// the constant value (imm) or the registers.
enum 
{
	OP_FLAG_UNKNOWN,
	OP_FLAG_IMMEDIATE,
	OP_FLAG_REGISTER
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
	for (size_t i = 0; i < vm->programLength; ++i)
		free(vm->program[i]);
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
void DecodeOperand(instruction_t *ins, int32_t operand)
{
	registers_t *reg = malloc(sizeof(registers_t));
	memset(reg, 0, sizeof(registers_t));
	ins->type = (operand >> 16) & 0xF;
	reg->r0   = (operand >> 12) & 0xF;
	reg->r1   = (operand >>  8) & 0xF;
	reg->r2   = (operand >>  4) & 0xF;
	reg->imm  = (operand & 0xFF)     ;
	ins->reg  = reg;
}

// Decode an instruction from our program loaded in memory and 
// translate that into a struct.
instruction_t *DecodeInstruction(vm_t *vm)
{
	if (++vm->ip > vm->programLength)
	{
		fprintf(stderr, "Error: %s tried to run past length of program. Terminating.\n", vm->name);
		vm->running = 0;
		return NULL;
	}
	
	instruction_t *ins = malloc(sizeof(instruction_t));
	memset(ins, 0, sizeof(instruction_t));
	
// 	printf("instruction pointer: %zu\n", vm->ip);
	ins->opcode = vm->program[vm->ip]->opcode;
 	printf("Running instruction \"0x%.4X\" at ip: %zu\n", ins->opcode, vm->ip);
	DecodeOperand(ins, vm->program[vm->ip]->operands);
	
	return ins;
}

static inline int has_even_parity(uint32_t x)
{
	uint32_t count = 0;
	
	for(uint32_t i = 0; i < (sizeof(uint32_t) * CHAR_BIT); i++)
		if(x & (1 << i))
			count++;
	
	return !(count % 2);
}

// Check and make sure the last operation doesn't need
// any new flags to be set.
void CheckFlags(vm_t *vm, int32_t value)
{
	if (value == 0)
		SETFLAGS(vm->regs[4], FLAG_ZERO);
	else
		UNSETFLAGS(vm->regs[4], FLAG_ZERO);
	
	// Thanks stackoverflow! Anyway, this checks if the integer is strictly positive
	// see http://stackoverflow.com/a/3731575 for more.
	uint32_t u = (uint32_t)value;
	int32_t strictly_positive = (-u & ~u) >> ((sizeof(int) * CHAR_BIT) - 1);
	// Set the sign flag depending on the signedness of the integer.
	if (strictly_positive)
		UNSETFLAGS(vm->regs[4], FLAG_SIGN);
	else
		SETFLAGS(vm->regs[4], FLAG_SIGN);
	
	// Set parity flag
	if (has_even_parity(value))
		SETFLAGS(vm->regs[4], FLAG_PARITY);
	else
		UNSETFLAGS(vm->regs[4], FLAG_PARITY);
	
	// TODO:
	//     implement FLAG_CARRY
	//     implement FLAG_OVERFLOW
}

void interpret(vm_t *vm)
{
        // load 2-words (8 bytes) of data and interpret it
        // first word is the instruction and the second is
        // the operands for that instruction. This allows
        // for more registers to be used.
	instruction_t *ins = DecodeInstruction(vm);
	
	// In case we get an invalid length or something.
	if (!ins)
		return;
	
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
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] += ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] += vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_SUB:
			// Subtract values
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] -= ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] -= vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_DIV:
			// Divide values
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] /= ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] /= vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_XOR:
			// xor 2 registers
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] ^= ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] ^= vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_NOT:
			// bitwise not register
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] = ~ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] = ~vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_OR:
			// bitwise or registers
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] |= ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] |= vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_AND:
			// bitwise and registers
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] &= ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] &= vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_SHL:
			// bitshift left
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] <<= ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] <<= vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_SHR:
			// bitshift right
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] >>= ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] >>= vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_INC:
			// increment register
			vm->regs[ins->reg->r0]++;
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_DEC:
			// decrement register
			vm->regs[ins->reg->r0]--;
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_CMP:
			// compare 2 registers together
			if (ins->type == OP_FLAG_IMMEDIATE)
				CheckFlags(vm, (vm->regs[ins->reg->r0] == ins->reg->imm));
			else if(ins->type == OP_FLAG_REGISTER)
				CheckFlags(vm, (vm->regs[ins->reg->r0] == vm->regs[ins->reg->r1]));
			break;
		case OP_MOV:
			// move values from register to register
			// (and register to stack when stack implemented)
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->regs[ins->reg->r0] = ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->regs[ins->reg->r0] = vm->regs[ins->reg->r1];
			CheckFlags(vm, vm->regs[ins->reg->r0]);
			break;
		case OP_CALL:
			// Call a section of code.
			// This is basically a push + jmp call in one.
			
			// put the instruction pointer + 1 (the +1 is so when we
			// return, we're not gonna jump into the same call statement)
			// onto the stack then increment the stack pointer.
			vm->opstack[vm->regs[3]++] = vm->ip+1;
			
			// Jump in the switch statement to OP_JMP
			goto jmpopcode;
		case OP_RET:
			// This the opposite of call.
			// Get the previous run position from stack and decrement the stack pointer.
			vm->ip = vm->opstack[vm->regs[3]--];
			break; // return, next iteration by CPU will be at new position
		case OP_PUSH:
			// Push value onto stack
			// we'll treat register 4 as the stack pointer.
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->opstack[vm->regs[3]] = ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->opstack[vm->regs[3]] = vm->regs[ins->reg->r0];

			vm->regs[3]++;
			break;
		case OP_PUSHF:
			// Push the flags register to the stack
			vm->opstack[vm->regs[3]++] = vm->regs[4];
			break;
		case OP_POP:
			// pop value from stack
			vm->regs[ins->reg->r0] = vm->opstack[vm->regs[3]--];
			break;
		case OP_JMP:
jmpopcode:		// Jump always -- other conditional jumps go here for cleanness
			if (ins->type == OP_FLAG_IMMEDIATE)
				vm->ip = ins->reg->imm;
			else if(ins->type == OP_FLAG_REGISTER)
				vm->ip = vm->regs[ins->reg->r0];
			break;
		case OP_JNZ:
			// Jump if not zero
			if (!(vm->regs[4] & FLAG_ZERO))
				goto jmpopcode;
			break;
		case OP_JZ:
			// Jump if zero
			if (vm->regs[4] & FLAG_ZERO)
				goto jmpopcode;
			break;
		case OP_JS:
			// Jump if sign flag is set
			if (vm->regs[4] & FLAG_SIGN)
				goto jmpopcode;
			break;
		case OP_JNS:
			// jump if sign flag is not set
			if (!(vm->regs[4] & FLAG_SIGN))
				goto jmpopcode;
			break;
		case OP_JGT:
			// jump if value is greater than zero
			if ((vm->regs[4] & FLAG_ZERO) || (!(vm->regs[4] & FLAG_SIGN) && !(vm->regs[4] & FLAG_OVERFLOW)))
				goto jmpopcode;
			break;
		case OP_JLT:
			// Jump if value is less than zero
			if ((vm->regs[4] & FLAG_SIGN) || (vm->regs[4] & FLAG_OVERFLOW))
				goto jmpopcode;
			break;
		case OP_JPE:
			// Jump if parity is even
			if (vm->regs[4] & FLAG_PARITY)
				goto jmpopcode;
			break;
		case OP_JPO:
			// Jump if parity is odd
			if (!(vm->regs[4] & FLAG_PARITY))
				goto jmpopcode;
			break;
		case OP_LEA:
		case OP_INT:
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
	
	// Exit the thread
	thrd_exit(0);
}

// Decode and compile the data into the struct above
void CompileVM(vm_t *vm, char *data, size_t len)
{
	// This loop is a bit lopsided because we're going from a char
	// (which is 1 byte) to two int32_t's (int32_t * 2 = 8 bytes).
	// So we may crash in this loop. Not sure how to handle this.

	
	// Make sure our program's opcodes are all valid. If they're not
	// then the loop below may crash or fail.
	if (len % sizeof(program_t) != 0)
	{
		fprintf(stderr, "WARNING: %s is not a multiple of %zu bytes in length,"
		                "likely invalid, mis-aligned, or corrupt program!\n",
			        vm->name, sizeof(program_t));
	}
	
	size_t instructions = len / sizeof(program_t);
	
	// Attempt to guess the number of instructions we're gonna be needing to allocate for.
	vm->program = calloc(instructions, sizeof(program_t*));
	
// 	printf("Preparing to compile program. Expecting %zu instructions. vm->program = 0x%p\n", instructions,
// 	       vm->program);
	
	size_t newlen = 0;
	while(newlen < len)
	{
		// Allocate a prorgam instruction
		program_t *pr = malloc(sizeof(program_t));
		
		// Determine the next best size to use to advance the copy
		size_t nextlen = MIN(sizeof(program_t), sizeof(program_t) - len - newlen);
		
		// copy it into the newly allocated pointer
		memcpy(pr, data, nextlen);
		
		newlen += nextlen;
		data += nextlen;
		vm->programLength++;
		
		if (vm->programLength > instructions)
		{
// 			printf("realloc'ing program length: %zu objects, size %zu\n", vm->programLength,
// 			       vm->programLength * sizeof(program_t*));
			void *tmpptr = realloc(vm->program, vm->programLength * sizeof(program_t*));
			if (!tmpptr)
			{
				fprintf(stderr, "failed realloc'ing %zu bytes: %s\n", vm->programLength * sizeof(program_t*),
					strerror(errno));
				exit(1);
			}
			vm->program = tmpptr;
		}
		
		vm->program[vm->programLength-1] = pr;
	}
}

// Obvious entry point.
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
	
	for (int i = 1; i < argc; ++i)
	{
		char *program = argv[i];
		size_t len = strlen(program);
		
		if (program[0] == '-') // Skip programs which start with '-' in their names.. might be an option
			continue;
		if (program[0] == '-' && program[1] == '-')
			continue;
		
		printf("Allocating vm struct for \"%s\"\n", program);
		vm_t *vm = AllocateVM();
		
		// Set our name and shit
		vm->name = program;
		vm->nameLen = len;
		// We're running
		vm->running = 1;
		
		printf("Attempting to open file \"%s\"\n", program);
		
		// Open a file as read-binary
		FILE *f = fopen(vm->name, "rb");
		
		if(!f)
		{
			fprintf(stderr, "Failed to open %s. Skipping.\n", vm->name);
			DeallocateVM(vm);
			continue;
		}
		
		printf("Program successfully opened, reading length and allocating temporary buffer\n");
		
		// Get program length
		fseek(f, 0, SEEK_END);
		size_t plen = ftell(f);
		rewind(f);
		
		printf("Program length: %zu bytes\n", plen);
		
		// Make sure our program is empty
		char *data = malloc(plen+1);
		// Make sure the whole buffer is null before we start
		memset(data, 0, plen+1);
		
		// Read each byte into our buffer
		size_t ret = fread(data, 1, plen, f);
		
		// Close our file descriptor
		fclose(f);
		
		printf("Verifying program length, fread returned %zu\n", ret);
		
		// Make sure the program has a valid length and isn't null.
		if (ret != plen || ret == 0)
		{
			fprintf(stderr, "Failed to read program: invalid length!\n");
			DeallocateVM(vm);
			continue;
		}
		
		printf("Compiling program into 8-byte segments\n");
		
		// Compile the VM
		CompileVM(vm, data, plen);
		
		printf("Cleaning up and continuing to next program...\n");
		
		// We don't need this anymore
		free(data);
		
		// Check if this is the first element in the linked list.
		if (!first)
			first = vm;
		// Set the previous vm so when we allocate our next
		// vm, we can add it to the linked list
		prev = vm;
	}
	
	printf("Starting program threads\n");
	
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
	
	printf("Waiting for threads to finish\n");
	
	// Unlock and let the threads do their thing.
	mtx_unlock(&listmutex);
	
	// WARNING: this is technically a race
	// condition but I am too lazy to write
	// code to handle it.
	//
	// Wait for the threads to finish.
	for (vm_t *vm = first; vm; vm = vm->next)
	{
		int res = 0;
		thrd_join(vm->thread, &res);
	}
	
	// We're done with the mutex
	mtx_destroy(&listmutex);
	
        return 0;
}
