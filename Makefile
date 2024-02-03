.PHONY: all

all: build

build: includes src/npm-review.cpp src/npm-review.h
	clang++ -std=c++11 -Wall -Werror -lncurses $(CFLAGS) src/*.cpp -o npm-review

install: build
	cp npm-review /usr/local/bin

watch: build
	@echo "Building done"

includes:
	@mkdir -p build/
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
		> build/$$(basename $$file .sh); \
	done

clean:
	rm -rf build/
