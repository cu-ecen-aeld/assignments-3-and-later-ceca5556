INCLUDE_DIRS = #-I/usr/include/opencv4
LIB_DIRS = 
TARGET = aesdsocket
CROSS_COMPILE = aarch64-none-linux-gnu-
CC=$(CROSS_COMPILE)gcc

CDEFS=
CFLAGS= -O0 -g -Wall -Werror #$(INCLUDE_DIRS) $(CDEFS)
LIBS= -lpthread #-L/usr/lib -lopencv_core -lopencv_flann -lopencv_video -lrt

HFILES= $(wildcard *.h)
CFILES= $(wildcard *.c)

#SRCS= ${HFILES} #${CFILES}
OBJS= ${CFILES:.c=.o}

.PHONY: default all clean

default: $(TARGET)
all:	default

clean:
	-rm -f *.o *.d
	-rm -f $(TARGET)
 
%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) -c $< -o $@


$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LIBS) -o $@