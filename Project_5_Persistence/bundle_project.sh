#!/bin/bash

# Script to bundle B-Tree project files into a single text file

OUTPUT_FILE="btree_project_bundle.txt"
SOURCE_FILES=("Makefile" "storage.c" "btree.c" "test_btree.c" "perf_btree.c" "main.c")

# Clear the output file if it exists
> "$OUTPUT_FILE"

echo "Bundling project files into $OUTPUT_FILE..."

for FILE in "${SOURCE_FILES[@]}"; do
  if [ -f "$FILE" ]; then
    echo "Appending $FILE..."
    echo "================================================================================" >> "$OUTPUT_FILE"
    echo "=== FILE: $FILE" >> "$OUTPUT_FILE"
    echo "================================================================================" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    cat "$FILE" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
  else
    echo "Warning: File $FILE not found, skipping."
  fi
done

echo "Bundling complete."