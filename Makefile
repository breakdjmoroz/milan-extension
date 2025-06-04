.PHONY: all clean run compile link build

PROGRAM=my_milan.bin

SOURCE_MILAN=example.mil
OBJECT_MILAN=$(patsubst %.mil,%,$(SOURCE_MILAN)).obj

DEBUG=y

SRC_DIR=src
INCLUDE_DIR=include
BUILD_DIR=bin
VM=./vm/bin/mvm

CC=g++
CFLAGS=$(INCLUDES) -Wall
ASFLAGS=$(INCLUDES)
LDFLAGS=
LIBS=

SRC_DIRS=$(shell find $(SRC_DIR) -type d)
INCLUDE_DIRS=$(shell find $(INCLUDE_DIR) -type d)
INCLUDES=$(foreach dir,$(INCLUDE_DIRS),$(patsubst %,-I%,$(dir)))

HEADERS=$(shell find $(INCLUDE_DIR) -name "*.h")

SOURCES_CPP=$(shell find $(SRC_DIR) -name "*.cpp")
OBJECTS_CPP=$(patsubst $(SRC_DIR)%.cpp,$(BUILD_DIR)%.o,$(SOURCES_CPP))

ifeq ($(DEBUG), y)
	CFLAGS:=$(CFLAGS) -g -pg
	LDFLAGS:=$(LDFLAGS) -pg
endif

all: build .vimrc

.vimrc:
	echo > .vimrc
	$(foreach dir, $(INCLUDE_DIRS),\
		echo set path+=$(dir) >> .vimrc;)

build: compile link

compile: $(OBJECTS_CPP)

.DELETE_ON_ERROR:
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	mkdir -p $(patsubst %$(shell basename $@),%,$@)
	$(CC) $(CFLAGS) -c $< -o $@

link: $(BUILD_DIR)/$(PROGRAM)

$(BUILD_DIR)/$(PROGRAM): $(OBJECTS_CPP)
ifneq ($(OBJECTS_CPP), )
		$(CC) $(LDFLAGS) $^ -o $(BUILD_DIR)/$(PROGRAM) $(LIBS)
endif

run:
	./$(BUILD_DIR)/$(PROGRAM) $(SOURCE_MILAN) > $(OBJECT_MILAN)
	$(VM) $(OBJECT_MILAN)

clean:
	rm -rd $(BUILD_DIR)/*
