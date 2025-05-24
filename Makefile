.PHONY: all build clean includes

CC := clang++
CFLAGS := -std=c++11 -Wall -Werror ${CFLAGS}
SRC_DIR := src
BUILD_DIR := build
EXECUTABLE := npm-review
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))

print_build = \
	printf "%-30s" $(1); \
	$(2); \
	echo "\x1b[32m✔\x1b[0m"

all: build

build: includes $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	@$(call print_build, "Building executable", \
		$(CC) $(CFLAGS) -lncurses $^ -o $@ \
	)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
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
test:
	@echo "Scripts"
	@./tests/run-tests.sh

clean:
	rm -rf build/
