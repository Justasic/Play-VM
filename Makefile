COMMONFLAGS=-Wall -Wextra -Wshadow -Wundef -pedantic -I. -g
# doing -D_BSD_SOURCE gets rid of a warning about strdup when using C11
CFLAGS=-std=c11 -D_BSD_SOURCE $(COMMONFLAGS)
CXXFLAGS=-std=c++11 $(COMMONFLAGS)
CC=clang
CXX=clang++
BUILDDIR=build

all: 
	mkdir -p $(BUILDDIR)
	@# Build our example program and link it to the GPK library
	$(CC) $(CFLAGS) -c main.c         -o $(BUILDDIR)/main.o
	$(CC) $(BUILDDIR)/*.o -o $(BUILDDIR)/playvm
	
clean:
	rm -rf $(BUILDDIR)/
