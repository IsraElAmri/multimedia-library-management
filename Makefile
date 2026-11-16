CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c99 -Iinclude -O2
TARGET = library_system

SRC = src/main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o *.obj *.exe *.dat
