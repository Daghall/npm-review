.PHONY: all

all: build

build: npm-review.cpp
	g++ -std=c++11 -lncurses npm-review.cpp -o npm-review

install: build
	cp npm-review /usr/local/bin
