CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -static-libgcc
PORT    ?= /dev/ttyACM0

SRC_DIR   := src
TEST_DIR  := tests
BUILD_DIR := build
ART_DIR   := artifacts

TEST_CRC32_BIN := $(BUILD_DIR)/test_crc32
TEST_MD5_BIN   := $(BUILD_DIR)/test_md5
PARSE_BIN      := $(BUILD_DIR)/ptable_parse
GEN_BIN        := $(BUILD_DIR)/ptable_gen

.PHONY: all run test verify clean

all: $(BUILD_DIR) $(TEST_CRC32_BIN) $(TEST_MD5_BIN) $(PARSE_BIN) $(GEN_BIN)

test: all
	@echo "══════════════════════════════"
	@echo "  TESTS CRC32"
	@echo "══════════════════════════════"
	$(TEST_CRC32_BIN)
	@echo "══════════════════════════════"
	@echo "  TESTS MD5"
	@echo "══════════════════════════════"
	$(TEST_MD5_BIN)

verify: test
	@echo "══════════════════════════════"
	@echo "  GÉNÉRATION ptable_gen.bin"
	@echo "══════════════════════════════"
	$(GEN_BIN) $(ART_DIR)/ptable_gen.bin
	@echo "══════════════════════════════"
	@echo "  VALIDATION vs ptable_ref.bin"
	@echo "══════════════════════════════"
	python3 tools/verify.py $(ART_DIR)/ptable_gen.bin $(ART_DIR)/ptable_ref.bin

run: verify
	@echo "══════════════════════════════"
	@echo "  FLASH → $(PORT)"
	@echo "══════════════════════════════"
	esptool.py --port $(PORT) write_flash 0x8000 $(ART_DIR)/ptable_gen.bin
	@echo "✅ Done."

clean:
	rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_CRC32_BIN): $(SRC_DIR)/crc32.c $(TEST_DIR)/test_crc32.c
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@

$(TEST_MD5_BIN): $(SRC_DIR)/md5.c $(TEST_DIR)/test_md5.c
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@

$(PARSE_BIN): $(SRC_DIR)/crc32.c $(SRC_DIR)/ptable_parse.c
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@

$(GEN_BIN): $(SRC_DIR)/md5.c $(SRC_DIR)/ptable_gen.c
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@
