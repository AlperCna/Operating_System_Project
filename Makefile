CC = gcc
CFLAGS = -Wall -g -pthread -lrt
TARGET = procx

all: $(TARGET)

$(TARGET): procx.c
	$(CC) $(CFLAGS) -o $(TARGET) procx.c

clean:
	rm -f $(TARGET)
