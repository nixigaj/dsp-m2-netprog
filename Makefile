.PHONY: all clean

TARGET = m2-netprog
SRC = $(wildcard *.c)
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

all: $(TARGET)

build-dir:
	@mkdir -p $(BUILD_DIR)

bin-dir:
	@mkdir -p $(BIN_DIR)

$(TARGET): bin-dir $(SRC)
	@gcc -g $(SRC) -o $(BIN_DIR)/$(TARGET)

clean:
	@rm -rf $(BUILD_DIR)
