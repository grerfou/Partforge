CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11
PORT    ?= /dev/ttyACM0

SRC_DIR   := src
TEST_DIR  := tests
BUILD_DIR := build
ART_DIR   := artifacts

TEST_BIN  := $(BUILD_DIR)/test_crc32
PARSE_BIN := $(BUILD_DIR)/ptable_parse

.PHONY: all run clean

all: $(BUILD_DIR) $(TEST_BIN) $(PARSE_BIN)

run: all
	@echo "══════════════════════════════"
	@echo "  TESTS"
	@echo "══════════════════════════════"
	$(TEST_BIN)
	@echo "══════════════════════════════"
	@echo "  FLASH → $(PORT)"
	@echo "══════════════════════════════"
	esptool.py --port $(PORT) write_flash 0x8000 $(ART_DIR)/ptable_ref.bin
	@echo "✅ Done."

clean:
	rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_BIN): $(SRC_DIR)/crc32.c $(TEST_DIR)/test_crc32.c
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@

$(PARSE_BIN): $(SRC_DIR)/crc32.c $(SRC_DIR)/ptable_parse.c
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@
