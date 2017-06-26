CC = gcc
CFLAGS  = -std=c99 -fPIC -Wall

VMAJOR = 0
VMINOR = 1

ALL: static dynamic

static: libOpenDMX.a

dynamic: libOpenDMX.so

libOpenDMX.a: LinkedList.o OpenDMX.o
	ar rcs -o libOpenDMX.a LinkedList.o OpenDMX.o

libOpenDMX.so: LinkedList.o OpenDMX.o
	gcc -shared -Wl,-soname,libOpenDMX.so.$(VMAJOR) -o libOpenDMX.so.$(VMAJOR).$(VMINOR)  LinkedList.o OpenDMX.o

OpenDMX.o: LinkedList.o OpenDMX.c LinkedList.h OpenDMX.h

LinkedList.o: LinkedList.c LinkedList.h
