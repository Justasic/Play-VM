#include <stdio.h>
 
// 3 registers for us to use
#define NUM_REGS 4
unsigned regs[NUM_REGS];

// OP codes
enum
{
	OP_UNUSED = 0,
	OP_NOOP   = 0x01,
	OP_HALT   = 0x02,
	OP_LOADI  = 0x03,
	OP_ADD    = 0x04,
	OP_SUB    = 0x05,
	OP_DIV    = 0x06
};

// Op code tables
typedef struct
{
	int opcode;
	const char *opcodename;
} opcodes_t;

static const opcodes_t OpTable[] = {
	{OP_UNUSED, "Unused"},
	{OP_NOOP,   "NOOP"},
	{OP_HALT,   "HALT"},
	{OP_LOADI,  "LOADI"},
	{OP_ADD,    "ADD"},
	{OP_SUB,    "SUB"},
	{OP_DIV,    "DIV"}
};
 
unsigned program[] =
{
	0x1064,    // loadi r0, 100
	0x11C8,    // loadi r1, 200
	0x2201,    // add r2, r0, r1
	0x0000     // halt
};
 
/* program counter */
int pc = 0;
 
/* fetch the next word from the program */
int fetch()
{
	if (pc > sizeof(program))
	{
		printf("WARNING: unterminated program!\n");
		return OP_HALT;
	}
	return program[pc++];
}
 
/* instruction fields */
int instrNum = 0;
int reg1     = 0;
int reg2     = 0;
int reg3     = 0;
int imm      = 0;
 
/* decode a word */
void decode(int instr)
{
	instrNum = (instr & 0xF000) >> 12;
	reg1     = (instr & 0xF00 ) >>  8;
	reg2     = (instr & 0xF0  ) >>  4;
	reg3     = (instr & 0xF   );
	imm      = (instr & 0xFF  );
}
 
/* the VM runs until this flag becomes 0 */
int running = 1;
 
/* evaluate the last decoded instruction */
void eval()
{
	// In case we hit an unknown instruction.
	if (instrNum <= sizeof(OpTable))
		printf("%s {r0: %d, r1: %d, r2: %d}\n", OpTable[instrNum].opcodename, reg1, reg2, reg3);
	
	switch(instrNum)
	{
		case OP_UNUSED:
			// ignore but print warning
			printf("Unused opcode encountered... Ignoring!\n");
		case OP_NOOP:
			// No-Operation
			break;
		case OP_HALT:
			/* halt */
			running = 0;
			break;
		case OP_LOADI:
			/* loadi */
// 			printf("loadi r%d #%d\n", reg1, imm);
			regs[reg1] = imm;
			break;
		case OP_ADD:
			/* add */
// 			printf("add r%d r%d r%d\n", reg1, reg2, reg3);
			regs[reg1] = regs[reg2] + regs[reg3];
			break;
		case OP_SUB:
			regs[reg1] = regs[reg2] - regs[reg3];
			break;
		case OP_DIV:
			regs[reg1] = regs[reg2] / regs[reg3];
			break;
		default:
			printf("Unknown instruction %d\n", instrNum);
			printf("halting.\n");
			running = 0;
			break;
	}
}
 
/* display all registers as 4-digit hexadecimal words */
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
	while(running)
	{
		showRegs();
		int instr = fetch();
		decode(instr);
		eval();
	}
	showRegs();
}
 
int main(int argc, const char *argv[])
{
	run();
	return 0;
}

