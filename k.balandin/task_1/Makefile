CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2
TARGET  = myprog
SRC     = myprog.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
