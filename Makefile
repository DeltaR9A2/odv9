
# Linux Build #################
export OS := linux
export CC := gcc
export PC := pkg-config

# Windows Build ###############
# export OS := windows
# export CC := x86_64-w64-mingw32-gcc
# export PC := x86_64-w64-mingw32-pkg-config

###############################
export PACKAGES := sdl2
export TARGET := game
export CFLAGS := -std=c11 -g -Wall -Wextra -Wfatal-errors -Wno-format-truncation -O2 -Iinc `$(PC) --cflags $(PACKAGES)` 
export LFLAGS := `$(PC) --libs $(PACKAGES)` -lGL -lm
export REMOVE  := rm -rf
###############################
 
SOURCES := $(wildcard ./src/*.c)
OBJECTS := $(SOURCES:./src/%.c=./obj/%.o)

RESSRC := $(wildcard ./res/*)
RESBIN := $(RESSRC:./res/%=./bin/%)

.PHONY: all clean run $(TARGET) debug

all: $(TARGET)

./obj/%.o: ./src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

./bin/%: ./res/%
	cp $< $@

$(TARGET): $(OBJECTS) $(RESBIN)
	$(CC) ./obj/*.o $(LFLAGS) -o ./bin/$@

run: $(TARGET)
	@(cd bin/ && exec ./$(TARGET))

clean:
	$(REMOVE) ./obj/*
	$(REMOVE) ./bin/*
