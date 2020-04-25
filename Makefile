CC=gcc
CFLAGS=-g -Wall
OBJS=example.o libconsistent.o md5.o
TARGET=example.out

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

clean:
	rm -f *.o
	rm -f $(TARGET)
