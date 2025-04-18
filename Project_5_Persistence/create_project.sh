#!/bin/bash

# Script to create the project directory structure and empty files
# for the headerless ANSI C B-Tree library.

echo "Creating project structure..."

# Create the source files (empty)
echo "Creating source files..."
touch storage.c
touch btree.c
touch test_btree.c
touch perf_btree.c
touch main.c

# Create the Makefile (empty)
echo "Creating Makefile..."
touch Makefile

# Optional: Add execute permissions to the script itself
# chmod +x create_project.sh

echo "Project structure created successfully:"
ls -l

echo ""
echo "Next steps:"
echo "1. Populate the .c files with the provided code skeletons."
echo "2. Populate the Makefile with the provided content."
echo "3. Compile using 'make'."
echo "4. Run tests using 'make test' or 'make ci'."
echo "5. Run the example using './main_btree'."
echo "6. Run performance tests using 'make perf'."

exit 0