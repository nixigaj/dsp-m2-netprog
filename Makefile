.PHONY: all release run run-valgrind clean

TARGET = m2-netprog
SRC = $(wildcard *.c)
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

all: $(TARGET)

release: $(TARGET)-release

build-dir:
	@mkdir -p $(BUILD_DIR)

bin-dir:
	@mkdir -p $(BIN_DIR)

$(TARGET): bin-dir $(SRC)
	@gcc -g $(SRC) -o $(BIN_DIR)/$(TARGET)

run: $(TARGET)
	@./$(BIN_DIR)/$(TARGET)

run-valgrind: $(TARGET)
	@valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./$(BIN_DIR)/$(TARGET)

clean:
	@rm -rf $(BUILD_DIR)

$(TARGET)-release: bin-dir $(SRC)
	@gcc -O2 $(SRC) -o $(BIN_DIR)/$(TARGET)
