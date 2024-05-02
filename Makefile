CC = gcc
CPPFLAGS = -std=c++11
CHECKFLAGS = -Wall -Wextra -Werror

.PHONY: all build run_client clean check

all: clean build run_server

build:
	@$(CC) $(FLAGS) $(CPPFLAGS) main.cpp proxy.cpp -o proxy_server -lstdc++

run_server:
	@./proxy_server 22222 5432 127.0.0.1

run_client:
	@psql "sslmode=disable host=127.0.0.1 port=22222 user=postgres dbname=postgres"

clean:
	@rm -rf proxy_server file.log

check:
	@clang-format -n *.cpp *.h


