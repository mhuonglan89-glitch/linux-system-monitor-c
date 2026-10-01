CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -pthread
BIN_DIR = bin
SRC_DIR = src

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(BIN_DIR)/server: $(SRC_DIR)/server.c
	@mkdir -p $(BIN_DIR) logs reports
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR)/client: $(SRC_DIR)/client.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BIN_DIR)/* received_file.bin logs/* reports/*
