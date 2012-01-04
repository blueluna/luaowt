LIBNAME = ks0066
LUADIR = /usr/include/lua5.1/

COPT = -O2 -DNDEBUG

CWARNS = -Wall -Wextra -pedantic \
        -Waggregate-return \
	-Wbad-function-cast \
        -Wcast-align \
        -Wcast-qual \
	-Wdeclaration-after-statement \
	-Wdisabled-optimization \
        -Wmissing-prototypes \
        -Wnested-externs \
        -Wpointer-arith \
        -Wshadow \
	-Wsign-compare \
	-Wstrict-prototypes \
	-Wundef \
        -Wwrite-strings \
	#  -Wunreachable-code \


CFLAGS = $(CWARNS) $(COPT) -std=c99 -I$(LUADIR)
CC = gcc

# For Linux
DLLFLAGS = -shared -fpic
ENV = 

# For Mac OS
# ENV = MACOSX_DEPLOYMENT_TARGET=10.4
# DLLFLAGS = -bundle -undefined dynamic_lookup

all: ks0066.so sysinfo.so

ks0066.so:	ks0066.o
	env $(ENV) $(CC) $(DLLFLAGS) ks0066.o -o ks0066.so

ks0066.o:	Makefile ks0066.c

sysinfo.so:	sysinfo.o
	env $(ENV) $(CC) $(DLLFLAGS) sysinfo.o -o sysinfo.so

sysinfo.o:	Makefile sysinfo.c

