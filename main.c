#include <stdio.h>
#include <unistd.h>
#include <string.h>

// our instruction macro (makes it easier to write instructions)
#define INSTR(instr, r0, r1, r2, imm) ((instr << 16) | (r0 << 12) | (r1 << 8) | (r2 << 4) | imm)
 
// 4 registers for us to use -- unfortunately,
// we cannot load all 4 registers into a single 32-bit word (it would requre 34 bits to do so)
// Expanding these beyond 4 will require a rewrite of how the format works.
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
	OP_AND    = 0x10, // bitwise and register(s)
	OP_SHL    = 0x11, // bitwise shift left register
	OP_SHR    = 0x12, // bitwise shift right register
	OP_INC    = 0x13, // increment register
	OP_DEC    = 0x14, // decrement register
	OP_CMP    = 0x15, // compare 2 registers
	
	// Not implemented -- requres stack
	OP_CALL   = 0x20, // Call a function
	OP_RET    = 0x21, // return from a function call
	OP_PUSH   = 0x22, // push value from register to stack
	OP_POP    = 0x23, // pop value from stack to register
	OP_JMP    = 0x24, // jump always to location
	OP_JNZ    = 0x25, // jump if not zero to location
	OP_JZ     = 0x26, // jump if zero to location
	
	
	// Extra opcodes
	OP_PRNT   = 0x18, // Temporary print opcodes.
	OP_DMP    = 0x19,
	OP_HUGE   = 0x100 // Test a really large opcode
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
	
	{OP_HUGE,   "HUGE!"}
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
	INSTR(OP_HALT,  0, 0, 0, 0),    // halt
	INSTR(OP_HUGE,  0, 0, 0, 0)
};

// program counter
int pc = 0;

// fetch the next word from the program
int fetch()
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
#ifdef OLD
	instrNum = (instr & 0xF0000) >> 16;
	reg1     = (instr & 0xF000 ) >> 12;
	reg2     = (instr & 0xF00  ) >>  8;
	reg3     = (instr & 0xF0   ) >>  4;
	imm      = (instr & 0xFF   );
#else
	instrNum = (instr >> 16) & 0xFF;
	reg1     = (instr >> 12) & 0xF ;
	reg2     = (instr >>  8) & 0xF ;
	reg3     = (instr >>  4) & 0xF ;
	imm      = (instr & 0xFF);
#endif
}

// the VM runs until this flag becomes 0
int running = 1;

// evaluate the last decoded instruction
void eval()
{
	// In case we hit an unknown instruction.
	if (instrNum <= (sizeof(OpTable) / sizeof(*OpTable)))
		printf("%s(%d) {r0: %d, r1: %d, r2: %d} imm %d\n", OpTable[instrNum].opcodename, instrNum, reg1, reg2, reg3, imm);
	
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
			
		// unimplemented opcodes
		case OP_CALL:
		case OP_RET:
		case OP_PUSH:
		case OP_POP:
		case OP_JMP:
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
void showRegs()
{
	int i;
	printf("regs = ");

	for(i = 0; i < NUM_REGS; i++)
		printf("%04X ", regs[i]);

	printf( "\n" );
}

void run()
{
	printf("Program length: %lu instructions\n", sizeof(program) / sizeof(*program));
	printf("Program size: %lu bytes\n", sizeof(program));
	while(running)
	{
		showRegs();
		int instr = fetch();
		decode(instr);
		eval();
		// Slow down our program so we can debug easier
		sleep(1);
	}
	showRegs();
}

void dumpProg()
{
	for (unsigned i = 0; i < (sizeof(program) / sizeof(*program)); ++i)
		printf("instr: 0x%X\n", program[i]);
}

int main(int argc, const char *argv[])
{
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
	
	run();
	return 0;
}
