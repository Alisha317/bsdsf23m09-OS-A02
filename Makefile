# Makefile for PA-02 (ls utility)
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
VERSION ?= 1.0.0

SRC    = src/lsv$(VERSION).c
OBJ    = obj/lsv$(VERSION).o
TARGET = bin/ls

all: $(TARGET)

# link step: object file -> executable
$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)
	@echo "Build complete: $(TARGET)"

# compile step: source file -> object file
$(OBJ): $(SRC)
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

run: all
	./$(TARGET)

clean:
	rm -f obj/*.o $(TARGET)

.PHONY: all run clean
