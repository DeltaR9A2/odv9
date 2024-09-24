
# Standard Build ###############################################################
# export OS := linux
export CC := gcc
export PC := pkg-config
export PACKAGES := sdl2
# export CFLAGS := -std=c11 -O2 `$(PC) --cflags $(PACKAGES)` -Wfatal-errors
export CFLAGS := -std=c11 -ggdb `$(PC) --cflags $(PACKAGES)` -Wfatal-errors
export LFLAGS := -Wl,-rpath='$$ORIGIN/lib' `$(PC) --libs $(PACKAGES)` -lm
export REMOVE  := rm -rf

################################################################################

SRC_DIR ?= ./src
OBJ_DIR ?= ./obj
BIN_DIR ?= ./bin
RES_DIR ?= ./res
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

################################################################################

export TARGET := game

all: $(TARGET)

./obj/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	cp -v $(RES_DIR)/* $(BIN_DIR)/
	$(CC) $(OBJECTS) $(CFLAGS) $(LFLAGS) -o $(BIN_DIR)/$@

################################################################################

.PHONY: clean run

clean:
	$(REMOVE) $(OBJ_DIR)/* $(BIN_DIR)/*

run: $(TARGET)
	@(cd ./bin/ && ./$(TARGET))
