#include <stdio.h>
#include <unistd.h>
#include <string.h>

// our instruction macro (makes it easier to write instructions)
#define INSTR(instr, r0, r1, r2, imm) ((instr << 16) | (r0 << 12) | (r1 << 8) | (r2 << 4) | imm)
 
// 4 registers for us to use -- unfortunately,
// we cannot load all 4 registers into a single 32-bit word (it would requre 34 bits to do so)
// Expanding these beyond 4 will require a rewrite of how the format works.
// The 4th register (known as regs[4]) is treated as the stack pointer.
// This value will be incremented and decremented as per the position in the stack,
// this will be used for calling functions and stuff
#define NUM_REGS 4
unsigned regs[NUM_REGS];

// OP codes
enum
{
	OP_UNUSED = 0x00, // Unused op -- should throw warning if used
	OP_NOOP   = 0x01, // No operation
	OP_HALT   = 0x02, // halt the execution of the program (eg, stop running)
	OP_LOADI  = 0x03, // load value into register
	OP_ADD    = 0x04, // add values
	OP_SUB    = 0x05, // Subtract values
	OP_DIV    = 0x06, // divide values
	OP_XOR    = 0x07, // bitwise exclusive or register(s)
	OP_NOT    = 0x08, // bitwise not register
	OP_OR     = 0x09, // bitwise or register(s)
	OP_AND    = 0x0A, // bitwise and register(s)
	OP_SHL    = 0x0B, // bitwise shift left register
	OP_SHR    = 0x0C, // bitwise shift right register
	OP_INC    = 0x0D, // increment register
	OP_DEC    = 0x0E, // decrement register
	OP_CMP    = 0x0F, // compare 2 registers
	OP_MOV    = 0x10, // move value from one register to another
	
	// Not implemented -- requres stack
	OP_CALL   = 0x11, // Call a function
	OP_RET    = 0x12, // return from a function call
	OP_PUSH   = 0x13, // push value from register to stack
	OP_POP    = 0x14, // pop value from stack to register
	OP_JMP    = 0x15, // jump always to location
	OP_JNZ    = 0x16, // jump if not zero to location
	OP_JZ     = 0x17, // jump if zero to location
	
	
	// Extra opcodes
	OP_PRNT   = 0x18, // Temporary print opcodes.
	OP_DMP    = 0x19,
};

// Op code tables
typedef struct
{
	int opcode;
	const char *opcodename;
} opcodes_t;

static const opcodes_t OpTable[] = {
	{OP_UNUSED, "Unused"},
	{OP_NOOP,   "NOOP"  },
	{OP_HALT,   "HALT"  },
	{OP_LOADI,  "LOADI" },
	{OP_ADD,    "ADD"},
	{OP_SUB,    "SUB"},
	{OP_DIV,    "DIV"},
	{OP_XOR,    "XOR"},
	{OP_NOT,    "NOT"},
	{OP_OR,     "OR" },
	{OP_AND,    "AND"},
	{OP_SHL,    "SHL"},
	{OP_SHR,    "SHR"},
	{OP_INC,    "INC"},
	{OP_DEC,    "DEC"},
	{OP_CMP,    "CMP"},
	{OP_MOV,    "MOV"},
	
	// Not implemented -- requres stack
	{OP_CALL,   "CALL"},
	{OP_RET,    "RET" },
	{OP_PUSH,   "PUSH"},
	{OP_POP,    "POP" },
	{OP_JMP,    "JMP" },
	{OP_JNZ,    "JNZ" },
	{OP_JZ,     "JZ"  },
	
	
	{OP_PRNT,   "PRNT"},
	{OP_DMP,    "DMP"},
};

// Our program
static const unsigned program[] =
{
	INSTR(OP_LOADI, 0, 0, 0, 100), // loadi r0, 100   -- load the constant value 100 into r0 register
	INSTR(OP_LOADI, 1, 0, 0, 200), // loadi r1, 200   -- load the constant value 200 into r1 register
	INSTR(OP_ADD,   2, 0, 1, 0),   // add r2 = r0, r1 -- add r0 and r1, place value in r2
	INSTR(OP_PRNT,  2, 0, 0, 0),   // prnt r2         -- Print the r2 register to terminal
	INSTR(OP_XOR,   2, 2, 2, 0),   // xor r2 = r2, r2 -- clear the r2 register
	INSTR(OP_PRNT,  2, 0, 0, 0),   // prnt r2         -- Print the r2 register to terminal
	INSTR(OP_SUB,   2, 1, 0, 0),   // sub r2 = r0, r1 -- Subtract r0 from r1, place value in r2
	INSTR(OP_DMP,   0, 0, 0, 0),   // dmp             -- Dump registers to terminal
	INSTR(OP_MOV,   2, 0, 0, 0),   // mov r2, r0
	INSTR(OP_MOV,   2, 1, 0, 0),   // mov r2, r1
        INSTR(OP_LOADI, 0, 0, 0, 18),  // loadi r0, 16
        INSTR(OP_CALL,  0, 0, 0, 0),   // call r0
        INSTR(OP_LOADI, 0, 0, 0, 12),  // loadi r0, 12    -- load number 12 to register r0
        //INSTR(OP_DMP,   0, 0, 0, 0),   // dmp             -- Dump that shit.
        INSTR(OP_JMP,   0, 0, 0, 0),   // jmp r0          -- Jump to above instruction
	INSTR(OP_HALT,  0, 0, 0, 0),    // halt


        INSTR(OP_UNUSED,0, 0, 0, 0),   // Test unused, should never trigger
        INSTR(OP_LOADI, 0, 0, 0, 321), // load random value
        INSTR(OP_LOADI, 1, 0, 0, 123), // another rand value
        INSTR(OP_ADD,   2, 0, 1, 0),   // add r2 = r0, r1
        INSTR(OP_RET,   0, 0, 0, 0),    // ret -- return to previous call location
        INSTR(OP_HALT,  0, 0, 0, 0)
};

// Our program stack
int opstack[sizeof(program) * sizeof(*program)];

// program counter
int pc = 0;

// fetch the next word from the program
int fetch(void)
{
	if (pc > (sizeof(program) / sizeof(*program)))
	{
		printf("WARNING: unterminated program!\n");
		return INSTR(OP_HALT, 0, 0, 0, 0);
	}
	return program[pc++];
}

// instruction fields
int instrNum = 0;
int reg1     = 0;
int reg2     = 0;
int reg3     = 0;
int imm      = 0;

// decode a word
void decode(int instr)
{
	instrNum = (instr >> 16) & 0xFF;
	reg1     = (instr >> 12) & 0xF ;
	reg2     = (instr >>  8) & 0xF ;
	reg3     = (instr >>  4) & 0xF ;
	imm      = (instr & 0xFF);
}

// the VM runs until this flag becomes 0
int running = 1;

// evaluate the last decoded instruction
void eval(void)
{
	// In case we hit an unknown instruction.
	if (instrNum <= (sizeof(OpTable) / sizeof(*OpTable)))
		printf("%s(%d) {r0: %d, r1: %d, r2: %d} imm %d\n",
                OpTable[instrNum].opcodename,
                instrNum, reg1, reg2, reg3, imm);
	
	switch(instrNum)
	{
		case OP_UNUSED:
			// ignore but print warning
			printf("Unused opcode encountered... ignoring!\n");
		case OP_NOOP:
			// No-Operation
			break;
		case OP_HALT:
			// halt the program
			running = 0;
			break;
		case OP_LOADI:
			// load static value into register
// 			printf("loadi r%d #%d\n", reg1, imm);
			regs[reg1] = imm;
			break;
		case OP_ADD:
			// Add values together
// 			printf("add r%d r%d r%d\n", reg1, reg2, reg3);
			regs[reg1] = regs[reg2] + regs[reg3];
			break;
		case OP_SUB:
			// Subtract values
			regs[reg1] = regs[reg2] - regs[reg3];
			break;
		case OP_DIV:
			// Divide values
			regs[reg1] = regs[reg2] / regs[reg3];
			break;
		case OP_XOR:
			// xor 2 registers
			regs[reg1] = regs[reg2] ^ regs[reg3];
			break;
		case OP_NOT:
			// bitwise not register
			regs[reg1] = ~regs[reg2];
			break;
		case OP_OR:
			// bitwise or registers
			regs[reg1] = regs[reg2] | regs[reg3];
			break;
		case OP_AND:
			// bitwise and registers
			regs[reg1] = (regs[reg2] & regs[reg3]);
			break;
		case OP_SHL:
			// bitshift left
			regs[reg1] = regs[reg2] << regs[reg3];
			break;
		case OP_SHR:
			// bitshift right
			regs[reg1] = regs[reg2] >> regs[reg3];
			break;
		case OP_INC:
			// increment register
			regs[reg1]++;
			break;
		case OP_DEC:
			// decrement register
			regs[reg1]--;
			break;
		case OP_CMP:
			// compare 2 registers together
			regs[reg1] = (regs[reg2] == regs[reg3]);
			break;
		case OP_MOV:
			// move values from register to register
			// (and register to stack when stack implemented)
			regs[reg2] = regs[reg1];
			break;
		case OP_CALL:
                        // Call a section of code.
                        // This is basically a push + jmp call in one.
                        opstack[regs[3]] = pc+1; // Push program position + 1 to stack -- the +1 is to move past the call instruction so we don't do an infinite loop
                        regs[3]++;             // Increment stack pointer
                        pc = regs[reg1];       // set program position
                        //printf("Saved location: %d -- jumping to location %d\n", opstack[regs[3]-1], pc);
                        break;                 // return, next iteration by CPU will be at the new position
		case OP_RET:
                        // This the opposite of call.
                        regs[3]--;             // Decrement stack pointer
                        pc = opstack[regs[3]]; // Get the previous run position from stack
                        //printf("Returning back to %d\n", pc);
                        break;                 // return, next iteration by CPU will be at new position
		case OP_PUSH:
                        // Push value onto stack
                        // we'll treat register 4 as the stack pointer.
                        opstack[regs[3]] = regs[reg1];
                        regs[3]++;
                        break;
		case OP_POP:
                        // pop value from stack
                        regs[reg1] = opstack[regs[3]];
                        regs[3]--;
                        break;
                case OP_JMP:
                        pc = regs[reg1];
                        break;
		case OP_JNZ:
		case OP_JZ:
			printf("Ignoring unimplemented opcode %d\n", instrNum);
			break;

		// Extended opcodes which will later be removed.
		case OP_PRNT:
			printf("r%d: %d\n", reg1, regs[reg1]);
			break;
		case OP_DMP:
			printf("Registers:\nr0: %d\nr1: %d\nr2: %d\nr3: %d\n", regs[0], regs[1], regs[2], regs[3]);
			break;
		default:
			// Handle unknown opcodes
			printf("Unknown instruction %d\n", instrNum);
			printf("halting.\n");
			running = 0;
			break;
	}
}

// display all registers as 4-digit hexadecimal words
void showRegs(void)
{
	int i;
	printf("regs = ");

	for(i = 0; i < NUM_REGS; i++)
		printf("%04X ", regs[i]);

	printf( "\n" );
}

void run(void)
{
	printf("Program length: %lu instructions\n", sizeof(program) / sizeof(*program));
	printf("Program size: %lu bytes\n", sizeof(program));
    memset(opstack, 0, sizeof(opstack));
	while(running)
	{
		showRegs();
		int instr = fetch();
		decode(instr);
		eval();
		// Slow down our program so we can debug easier
// 		sleep(1);
        usleep(8400);
	}
	showRegs();
}

void dumpProg(void)
{
	for (unsigned i = 0; i < (sizeof(program) / sizeof(*program)); ++i)
		printf("instr: 0x%X\n", program[i]);
}

int main(int argc, const char *argv[])
{
	// Handle initial arguments
	printf("Args length: %d\n", argc);
	for (int i = 0; i < argc; ++i)
		printf("argv[%d]: %s\n", i, argv[i]);
	
	if (argc >= 2)
	{
		if (!strcasecmp(argv[1], "--dump"))
		{
			dumpProg();
			return 0;
		}
	}
	
	// Run application
	run();
	// return success
	return 0;
}
