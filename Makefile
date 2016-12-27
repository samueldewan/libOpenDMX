CC = gcc
CFLAGS  = -std=c99 -Wall

VMAJOR = 0
VMINOR = 1

ALL: static dynamic

static: LinkedList.o OpenDMX.o
	ar rcs -o libOpenDMX.a LinkedList.o OpenDMX.o

dynamic: LinkedList.o OpenDMX.o
	gcc -shared -Wl,-soname,libOpenDMX.so.$(VMAJOR) -o libOpenDMX.so.$(VMAJOR).$(VMINOR)  LinkedList.o OpenDMX.o

OpenDMX.o: LinkedList.o OpenDMX.c

LinkedList.o: LinkedList.c
