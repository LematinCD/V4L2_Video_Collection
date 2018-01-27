INC= -Iinclude/
SRC=$(wildcard *.c)
OBJS=$(patsubst %.c,%.o,$(SRC))
LDFLAGS= -ljpeg -lpthread 
#CFLAGS= $(INC) -g -DJPEG -DDEBUG -DM0
CFLAGS= $(INC) -g -DYUYV -DDEBUG -DM0

ifeq ($(ARCH),arm)
BIN=client_arm
CC=arm-linux-gcc
else
BIN=client_Pi
CC=gcc
endif
$(BIN):$(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm $(OBJS) $(BIN)

