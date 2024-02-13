EXEC_NAME = scan_drive.exe

INC_DIR = include
SRC_DIR = src
OBJ_DIR = obj

EXECUTABLE = $(SRC_DIR)/main.c

OBJECTS = $(patsubst %.c, %.o, $(wildcard $(SRC_DIR)/*.c))
OBJECTS := $(subst $(SRC_DIR), $(OBJ_DIR), $(OBJECTS))
OBJECTS := $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS))
OBJECTS := $(filter-out $(OBJ_DIR)/debug.o, $(OBJECTS))

CC = gcc
CFLAGS = -Wall -Werror -std=c99 -I $(INC_DIR)

.PHONY: all
all: $(OBJECTS) $(EXECUTABLE)
	$(CC) $(CFLAGS) $(EXECUTABLE) $(OBJECTS) -o $(EXEC_NAME) -lm

.PHONY: refresh
refresh:
	touch ./* ./$(INC_DIR)/* ./$(OBJ_DIR)/* ./$(SRC_DIR)/*
	make clean

.PHONY: clean
clean:
	rm -f $(OBJ_DIR)/*.o *.exe

.PHONY: debug
debug: CFLAGS += -DDEBUG -g
debug: OBJECTS += $(OBJ_DIR)/debug.o
debug: $(OBJ_DIR)/debug.o all

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@ -lm