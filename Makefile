# Cody Jackson 
# CS333 
# 02/14/25
# Lab 2: UNIX File I/O - Arvik
# Makefile

CC = gcc
DEFINES =
DEBUG = -g3 -O0
CFLAGS = $(DEFINES) $(DEBUG) -Wall -Wextra -Wshadow -Wunreachable-code -Wredundant-decls \
		 -Wmissing-declarations -Wold-style-definition -Wmissing-prototypes -Wdeclaration-after-statement \
		 -Wno-return-local-addr -Wunsafe-loop-optimizations -Wuninitialized -Werror
PERMS = og-rx

PROG1 = arvik
PROGS = $(PROG1)

all: $(PROGS)

$(PROG1): $(PROG1).o
	$(CC) $(CFLAGS) -o $(PROG1) $(PROG1).o
	chmod $(PERMS) $(PROG1)

$(PROG1).o: $(PROG1).c $(PROG1).h
	$(CC) $(CFLAGS) -c $(PROG1).c

clean cls:
	rm -f $(PROGS) *.o *~ \#*

git-commit:
	if [ ! -d .git ] ; then git init; fi
	git add arvik.c Makefile
	git commit -m "Lab 2 Makefile Commit"

TAR_FILE = ${LOGNAME}_$(PROG1).tar.gz
tar:
	rm -f $(TAR_FILE)
	tar cvaf $(TAR_FILE) arvik.c Makefile
	tar tvaf $(TAR_FILE)

opt:
	make clean
	make DEBUG=-O
