#!/bin/env python3
#
# This is a assembler written in Python for the virtual machine
# included in this repo
#
import os
import sys
import struct

class CompilationError(Exception):
	def __init__(self, message):
		# Initialize exception class
		super().__init__(self, message)
		# set our message
		self.message = message
		
	def __unicode__(self):
		return self.message

# Opcodes table
mnemonics = {
	#OP_UNUSED = 0x000, // Unused -- throw error if used because program is likely corrupt.
        #OP_NOP    = 0x001, // No-operation opcode
        #OP_ADD    = 0x002, // add two numbers together
        #OP_SUB    = 0x003, // subtract two numbers
        #OP_MUL    = 0x004, // Multiply two numbers
        #OP_DIV    = 0x005, // divide two numbers
        
	'UNUSED': 0x000,
	'NOP':    0x001,
	'ADD':    0x002,
	'SUB':    0x003,
	'MUL':    0x004,
	'DIV':    0x005,
	
        #OP_XOR    = 0x006, // bitwise exclusive or
        #OP_OR     = 0x007, // bitwise or
        #OP_NOT    = 0x008, // bitwise not
        #OP_AND    = 0x009, // bitwise and
        #OP_SHR    = 0x00A, // bitshift right
        #OP_SHL    = 0x00B, // bitshift left

	'XOR':    0x006,
	'OR':     0x007,
	'NOT':    0x008,
	'AND':    0x009,
	'SHR':    0x00A,
	'SHL':    0x00B,
	
	'INC':    0x00C,
	'DEC':    0x00D,
	
        #OP_MOV    = 0x00E, // Move values from register to register
        #OP_CMP    = 0x00F, // Compare two registers
        #OP_CALL   = 0x010, // Call a function
        #OP_RET    = 0x011, // Return from a function call
        #OP_PUSH   = 0x012, // Push a value to the stack
        #OP_POP    = 0x013, // Pop a value from the stack
        #OP_LEA    = 0x014, // Load effective address
        
        'MOV':    0x00E,
	'CMP':    0x00F,
	'CALL':   0x010,
	'RET':    0x011,
	'PUSH':   0x012,
	'POP':    0x013,
	'LEA':    0x014,
	
        #OP_JMP    = 0x015, // Jump always
        #OP_JNZ    = 0x016, // Jump if not zero
        #OP_JZ     = 0x017, // Jump if zero
        #OP_JS     = 0x018, // Jump if sign
	#OP_JNS    = 0x019, // Jump if not sign
        #OP_JGT    = 0x01A, // Jump if greater than
        #OP_JLT    = 0x01B, // Jump if less than
	#OP_JPE    = 0x01C, // Jump if parity even
	#OP_JPO    = 0x01D, // Jump if parity odd
	
	'JMP':    0x015,
	'JNZ':    0x016,
	'JZ':     0x017,
	'JS':     0x018,
	'JNS':    0x019,
	'JGT':    0x01A,
	'JLT':    0x01B,
	'JPE':    0x01C,
	'JPO':    0x01D,

        #OP_HALT   = 0x01E, // Halt the application
        #OP_INT    = 0x01F, // Interrupt -- used for syscalls
	#OP_LOADI  = 0x020, // Load an imm value
	#OP_PUSHF  = 0x021, // Push flags to stack
	#OP_POPF   = 0x022, // Pop flags from stack
	
	'HALT':   0x01E,
	'INT':    0x01F,
	'LOADI':  0x020,
	'PUSHF':  0x021,
	'POPF':   0x022,
	
        #OP_DMP    = 0xA00, // Dump all registers to terminal
        #OP_PRNT   = 0xA01 // Dump specific register to the terminal
        
	'PRNT':   0xA00,
	'DMP':    0xA01,
}

pc = 0       # Our program counter (used for jump positions and such)
lc = 0       # Our line counter
program = [] # Our program (compiled)
labels = {}  # dict of labels and their locations.

def lookupMnemonic(mstr):
	try:
		return mnemonics[mstr.upper()]
	except:
		# Fail the compile
		raise CompilationError('Unknown mnemonic on line %d: \"%s\"' % (lc, mstr))

def compileMnemonic(instr, r0, r1, r2, r3, r4, imm, is_static):
	# This needs to be modified to allow all registers to be used.
	return [instr, ((int(is_static) << 16) | (r0 << 12) | (r1 << 8) | (r2 << 4) | imm)]

# Parse a single line of assembly
def parseMnemonic(line):
	global pc, lc, program, labels
	
	# Our registers
	regs = {}
	# our constant
	imm = 0
	# our label
	lbl = ''
	# check if the mnemonic's operands is a static value or a register
	is_static = False
	
	# Parsed a line so lets count it.
	lc += 1
	
	# Remove comments from line
	if line.find(';') != -1:
		line = line[:line.find(';')]
	
	# Normalize the strings
	line = line.strip().replace('  ', ' ')
	
	# Ignore empty lines
	if not line:
		return False
	
	# Ignore comments
	if line[0] == ';':
		return False
	
	print("Parsing line[%d]: \"%s\"" % (pc, line))
	
	# make sure our line isn't a label
	if line[len(line)-1] == ':':
		print('Label: "%s"' % (line[:-1]))
		labels[line[:-1]] = pc
		return False
		
	# get the opcode
	if line.find(' ') != -1:
		opcode, operands = line.split(' ', maxsplit=1)
		operands = operands.split(',')
	else:
		# Handle opcodes which have no operands (like dmp and halt)
		opcode = line
		operands = False
	
	print("OP: \"%s\" => 0x%.3X" % (opcode, lookupMnemonic(opcode.strip())))
	
	if operands:
		# get the operands
		print ("Operands: ", operands)
		for i in operands:
			i = i.strip()
			# compute operands
			if i[0] == 'r':
				# register, get it's value
				reg = int(i[1:])
				regs[reg] = int(reg)
				if reg > 3:
					print("Warning: \"%s\" directly modifies stack pointer register or flags regsiter!"
						% line);
			elif i[0] == '#':
				# constant value
				imm = int(i[1:])
				is_static = True
			elif i[0] == '$':
				# label
				imm = labels[i[1:]]+1
				is_static = True
			else:
				raise CompilationError("Unknown operand \"%s\" for mnemonic \"%s\" on line %d" % (i, opcode, lc))
		
	
	# Compile the assmebly
	program += compileMnemonic(
			lookupMnemonic(opcode.strip()),
			regs[0] if 0 in regs and regs[0] != False else 0, # r0 register
			regs[1] if 1 in regs and regs[1] != False else 0, # r1 register
			regs[2] if 2 in regs and regs[2] != False else 0, # r2 register
			regs[3] if 3 in regs and regs[3] != False else 0, # r3 register -- aka stack pointer
			regs[4] if 4 in regs and regs[4] != False else 0, # r4 regsiter -- aka flags
			imm, is_static)
	# Increment program counter
	pc += 1
	# return success
	return True
	

# Make it a function that we can import
def Assemble(filename, compiledfile):
	""" Our format for assembly is pretty simple.
	    We'll have the syntax look like this:
	    
	    [Optional Label:]
	    [<white space>]<mnemonic><white space><operand>[<white space>]<newline>
	    
	    where registers are referenced with prefixed 'r' and a number afterwards,
	    where constants are referenced with prefixed '#' and a value afterwards,
	    where labels are referenced with prefixed '%' and a label-name afterwards,
	    
	    This will make the assembly seem somewhat high-level, which is nice because
	    I hate hand-writing offsets.
	    """

	# Open a file descriptor of the file we want to compile.
	fd = open(filename, 'r+')
	# For each line, read the assembly.
	try:
		for line in fd:
			mn = parseMnemonic(line)
			if mn == False:
				continue
			else:
				pass # handle this later.
	except CompilationError as e:
		print("Failed to compile: %s" % e.message)
	else:
		if not compiledfile:
			print("Program: ", program)
		else:
			fd2 = open(compiledfile, 'wb')
			for i in program:
				# Write the program out as 4-byte integers
				fd2.write(struct.pack('i', i))
			fd2.close()
	fd.close()

if __name__ == "__main__":
	
	if len(sys.argv) == 1 or sys.argv[1].lower() == '-h' or sys.argv[1].lower() == '--help':
		print("Basic virtual machine assembler written by Justin Crawford\n")
		print("USAGE: %s [options] source [...] object\n" % sys.argv[0])
		print("OPTIONS:")
		print(" -h, --help          This message")
		sys.exit(1)
	
	Assemble(sys.argv[1], sys.argv[2])
