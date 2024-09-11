#template taken from https://stackoverflow.com/questions/1484817/how-do-i-make-a-simple-makefile-for-gcc-on-linux/1484873#1484873
INCLUDE_DIRS = #-I/usr/include/opencv4
LIB_DIRS = 
CROSS_COMPILE = #aarch64-none-linux-gnu-
TARGET = writer
CC=$(CROSS_COMPILE)gcc

CDEFS=
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LIBS= #-L/usr/lib -lopencv_core -lopencv_flann -lopencv_video -lrt

#HFILES= 
#CFILES= writer.c

#SRCS= ${HFILES} ${CFILES}
#OBJS= ${CFILES:.c=.o}

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)