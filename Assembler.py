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
	'UNUSED': 0x00,
	'NOP':    0x01,
	'HALT':   0x02,
	'LOADI':  0x03,
	'ADD':    0x04,
	'SUB':    0x05,
	'DIV':    0x06,
	'XOR':    0x07,
	'NOT':    0x08,
	'OR':     0x09,
	'AND':    0x0A,
	'SHL':    0x0B,
	'SHR':    0x0C,
	'INC':    0x0D,
	'DEC':    0x0E,
	'CMP':    0x0F,
	'MOV':    0x10,
	'CALL':   0x11,
	'RET':    0x12,
	'PUSH':   0x13,
	'POP':    0x14,
	'JMP':    0x15,
	'JNZ':    0x16,
	'JZ':     0x17,
	'JEQ':    0x18,
	'JNE':    0x19,
	'JGT':    0x1A,
	'JLT':    0x1B,
	'CALLF':  0x1C,
	
	
	'PRNT':   0x25,
	'DMP':    0x26,
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

def compileMnemonic(instr, r0, r1, r2, imm):
	return ((instr << 16) | (r0 << 12) | (r1 << 8) | (r2 << 4) | imm)

# Parse a single line of assembly
def parseMnemonic(line):
	global pc, lc, program, labels
	
	# Our registers
	regs = {}
	# our constant
	imm = 0
	# our label
	lbl = ''
	
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
	
	print("OP: \"%s\" => 0x%.2x" % (opcode, lookupMnemonic(opcode.strip())))
	
	if operands:
		# get the operands
		print ("Operands: ", operands)
		for i in operands:
			i = i.strip()
			# compute operands
			if i[0] == 'r':
				# register, get it's value
				reg = i[1:]
				regs[reg] = int(reg)
			elif i[0] == '#':
				# constant value
				imm = int(i[1:])
			elif i[0] == '$':
				# label
				imm = labels[i[1:]]+1
			else:
				raise CompilationError("Unknown operand \"%s\" for mnemonic \"%s\" on line %d" % (i, opcode, lc))
		
	
	# Compile the assmebly
	program.append(compileMnemonic(
			lookupMnemonic(opcode.strip()),
			regs[0] if 0 in regs and regs[0] != False else 0,
			regs[1] if 1 in regs and regs[1] != False else 0,
			regs[2] if 2 in regs and regs[2] != False else 0,
			imm)
		)
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
	Assemble(sys.argv[1], sys.argv[2])
