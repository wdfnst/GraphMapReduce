###########################################
#Makefile for simple programs
###########################################
INC= -Iinclude/metis/GKlib/
LIB= -lstdc++ #-Llib -lGKlib

CC= mpic++
CC_FLAG=-Wall -std=c++11

PRG=gmr
OBJ=examples.o 

$(PRG):
		$(CC) $(CC_FLAG) $(INC) $(LIB) -o $@ $@.cpp

testrwg:
		$(CC) $(CC_FLAG) $(INC) $(LIB) -o $@ $@.cpp
			
.SUFFIXES: .c .o .cpp
	.cpp.o:
		$(CC) $(CC_FLAG) $(INC) -c $*.cpp -o $*.o

clean:
	@echo "Removing linked and compiled files......"
		rm -f $(OBJ) $(PRG) testrwg
