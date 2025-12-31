# /c/cygwin64/bin/gcc.exe -Wall -Wextra -Wpedantic -pedantic-errors -fsanitize=undefined -fsanitize-trap -fsanitize=enum -fsanitize=bool -fsanitize=bounds -fanalyzer -std=c99 -D_POSIX_C_SOURCE=200809L -Og -g3 -o tcp_server.exe tcp_server.c

gcc -Wall -Wextra -Wpedantic -pedantic-errors -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -fsanitize=undefined -fsanitize-trap -fsanitize=enum -fsanitize=bool -fsanitize=bounds -fsanitize=address -std=c23 -Og -g3 -D_POSIX_C_SOURCE=200809L -o build/budgetoracle src/budgetoracle.c
