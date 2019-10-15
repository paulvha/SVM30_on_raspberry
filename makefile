###############################################################
# makefile for SVM30. October 2019 / paulvha
#
# To create a build with only the SVM30 monitor type: 
#		make
#
#
# To create a build with the SVM0 and SDS011 monitor type:
# 		make BUILD=SDS011
#
###############################################################
BUILD ?= svm30

# Objects to build
OBJ := svm30lib.o svm30.o
OBJ_SDS := sds011/serial.o sds011/sds011_lib.o sds011/sdsmon.o

# GCC flags
CXXFLAGS := -Wall -Werror -c
CC_CCS811 := -Dccs811
CC_SDS := -DSDS011

# set the right flags and objects to include
ifeq ($(BUILD),svm30)
fresh:

#include SDS011
else ifeq ($(BUILD),SDS011)
CXXFLAGS += $(CC_SDS) 
OBJ += $(OBJ_SDS)
fresh:

#others to add here
endif

# set variables
CC := gcc
DEPS := svm30lib.h bcm2835.h 
LIBS := -lbcm2835 -lm

# how to create .o from .c or .cpp files
.c.o: %c $(DEPS)
	$(CC) $(CXXFLAGS) -o $@ $<

.cpp.o: %c $(DEPS)
	$(CC) $(CXXFLAGS) -o $@ $<

.PHONY : clean svm30 fresh newsvm 
	
svm30 : $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

clean :
	rm -f svm30  sds011/sds011_lib.o sds011/serial.o sds011/sdsmon.o $(OBJ)

# sbm30.o is removed as this is only impacted by including
# SDS011 or not. 
newsvm :
	rm -f svm30.o
	
# first execute newsps then build svm30
fresh : newsvm svm30
