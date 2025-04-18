# Basic Makefile for ANSI C B-Tree Library (No Headers)

CC = gcc
CFLAGS = -ansi -Wall -Wpedantic -Werror
# Add -g for debugging, -O2 for optimization, etc.

BTREE_SRC = btree.c storage.c
BTREE_OBJ = $(BTREE_SRC:.c=.o)
TEST_SRC = test_btree.c
TEST_OBJ = $(TEST_SRC:.c=.o)
MAIN_SRC = main.c
MAIN_OBJ = $(MAIN_SRC:.c=.o)
PERF_SRC = perf_btree.c
PERF_OBJ = $(PERF_SRC:.c=.o)

TEST_EXE = test_btree
MAIN_EXE = main_btree
PERF_EXE = perf_btree

all: $(MAIN_EXE) $(TEST_EXE) $(PERF_EXE)

$(MAIN_EXE): $(MAIN_OBJ) $(BTREE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_EXE): $(TEST_OBJ) $(BTREE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(PERF_EXE): $(PERF_OBJ) $(BTREE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# Modified Object file compilation rule - no header dependencies listed
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# CI Target
ci: $(TEST_EXE)
	@echo "Running CI tests..."
	./$(TEST_EXE)
	@echo "CI tests completed successfully."

# Performance Target
perf: $(PERF_EXE)
	@echo "Running performance harness..."
	./$(PERF_EXE)
	@echo "Performance harness completed."

# Clean Target
clean:
	@echo "Cleaning up..."
	rm -f $(BTREE_OBJ) $(TEST_OBJ) $(MAIN_OBJ) $(PERF_OBJ)
	rm -f $(TEST_EXE) $(MAIN_EXE) $(PERF_EXE)
	rm -f *.db *.o core

.PHONY: all clean test ci perf