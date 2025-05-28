.PHONY: all build clean includes test

CC := clang++
CFLAGS := -std=c++17 -Wall -Werror ${CFLAGS}
SRC_DIR := src
BUILD_DIR := build
EXECUTABLE := npm-review
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))

TEST_SRC_DIR := tests/test-runner
TEST_BUILD_DIR := build/test-runner
TEST_EXECUTABLE := test-runner
TEST_SOURCES := $(wildcard $(TEST_SRC_DIR)/*.cpp)
TEST_OBJECTS := $(patsubst $(TEST_SRC_DIR)/%.cpp, $(TEST_BUILD_DIR)/%.o, $(TEST_SOURCES))

print_build = \
	printf "%-35s" $(1); \
	$(2); \
	echo "\x1b[32m✔\x1b[0m"

all: build test-runner

build: includes $(EXECUTABLE)


# Build the executable
$(EXECUTABLE): $(OBJECTS)
	@$(call print_build, "Building npm-review", \
		$(CC) $(CFLAGS) -lncurses $^ -o $@ \
	)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@$(call print_build, "Compiling $(<F)",\
		$(CC) $(CFLAGS) -c $< -o $@ \
	)


# Build the test-runner
$(TEST_EXECUTABLE): $(TEST_OBJECTS)
	@$(call print_build, "Building test-runner", \
		$(CC) $(CFLAGS) $^ -o $@ \
	)

$(TEST_BUILD_DIR)/%.o: $(TEST_SRC_DIR)/%.cpp
	@mkdir -p $(TEST_BUILD_DIR)
	@$(call print_build, "Compiling $(<F)",\
		$(CC) $(CFLAGS) -c $< -o $@ \
	)


debug: build
debug: CFLAGS += -g

install: build
	cp npm-review /opt/homebrew/bin/

_watch: debug
	@echo ""

watch:
	@printf "Watching for changes...\n\n"
	@fswatch src/ --event Updated --one-per-batch | xargs -I @ make _watch

watch-tests: TEST_FLAGS += --break --no-exit-code
watch-tests: test
	@printf "Watching tests for changes...\n\n"
	@fswatch tests/ src/ scripts/ --event Updated --one-per-batch | TEST_FLAGS="$(TEST_FLAGS)" xargs -I @ make test

includes:
	@mkdir -p $(BUILD_DIR)
	@for file in $$(ls scripts); do \
		sed -n '/# SCRIPT/,//p' scripts/$$file | \
		sed -E \
			-e '1s/.*/STR(/' \
			-e '$$s/.*/&)/' \
			-e 's/%/%%/g' \
			-e 's/\$$[a-z_]{2,}/%s/g' \
			-e 's/\\$$//g' | \
		tr -d '\n' \
		\
		> $(BUILD_DIR)/$$(basename $$file .sh); \
	done

t: test
test: test-runner
	@clear
	@echo "Scripts"
	@./tests/run-tests.sh
	@echo "TUI"
	@./test-runner $(TEST_FLAGS)

clean:
	rm -rf build/
