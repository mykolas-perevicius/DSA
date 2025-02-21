#!/bin/bash

###############################################################################
# test.sh - A more robust test script for blockade.c
#
# Enhancements:
# 1. Allows you to run *all* tests or just *one* specific input file.
# 2. Stores program outputs to temporary files, making diff easier to handle.
# 3. Does not exit immediately on the first failed test (unless you want that).
# 4. Summarizes how many tests passed/failed.
# 5. Maintains -u and -o pipefail but removes -e so you see all test results.
###############################################################################

set -u          # Treat unset variables as errors
set -o pipefail # Pipelines fail if *any* command fails
# set -e        # <--- Commented out so script continues even if a test fails
shopt -s nullglob

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE="$SCRIPT_DIR/blockade.c"
EXEC="$SCRIPT_DIR/blockade"

###############################################################################
# HELPER FUNCTION: Print usage information
###############################################################################
usage() {
  echo "Usage: $0 [all | <input_file.txt>]"
  echo
  echo "Examples:"
  echo "  $0 all                  # Test all input*.txt files"
  echo "  $0 input.txt            # Test only 'input.txt'"
  exit 1
}

###############################################################################
# PARSE ARGUMENTS
# If no arguments, or 'all', we run all tests. Otherwise, run with the specified file.
###############################################################################
RUN_ALL=true
SINGLE_INPUT=""

if [ $# -eq 1 ]; then
  if [ "$1" != "all" ]; then
    RUN_ALL=false
    SINGLE_INPUT="$1"
    if [ ! -f "$SINGLE_INPUT" ]; then
      echo "Error: File '$SINGLE_INPUT' not found."
      usage
    fi
  fi
elif [ $# -gt 1 ]; then
  usage
fi

###############################################################################
# COMPILE blockade.c
###############################################################################
if [ ! -f "$SOURCE" ]; then
  echo "Error: Source file not found: $SOURCE"
  exit 1
fi

echo "Compiling $SOURCE ..."
gcc -ansi -Wall -Wextra -Wpedantic -Werror "$SOURCE" -o "$EXEC"
echo "Compilation successful."
echo

###############################################################################
# COLLECT INPUT FILES
###############################################################################
# If RUN_ALL = true, collect all input*.txt files in SCRIPT_DIR.
# If RUN_ALL = false, just test the SINGLE_INPUT.

INPUT_FILES=()
if [ "$RUN_ALL" = true ]; then
  # Grab all matching input*.txt files
  for f in "$SCRIPT_DIR"/input*.txt; do
    [ -f "$f" ] && INPUT_FILES+=( "$f" )
  done
  if [ ${#INPUT_FILES[@]} -eq 0 ]; then
    echo "No input*.txt files found in $SCRIPT_DIR."
    exit 0
  fi
else
  # Just use the single input file the user specified
  INPUT_FILES=( "$SINGLE_INPUT" )
fi

###############################################################################
# RUN TESTS
###############################################################################
passed=0
failed=0

for input in "${INPUT_FILES[@]}"; do
  # Derive the expected output file name
  expected="${input/input/output}"

  echo "--------------------------------------------------"
  echo "Running test with input: $(basename "$input")"

  # Run the program and capture its output in a temp file
  tmp_output="$(mktemp)"
  "$EXEC" "$input" >"$tmp_output" 2>&1
  
  # If there's no expected file, create it from the current output
  if [ ! -f "$expected" ]; then
    echo "No expected file found. Creating: $expected"
    cp "$tmp_output" "$expected"
    # We won't treat this as fail or pass. It's effectively "generated."
    echo "Generated expected output from this run."
    echo
    continue
  fi

  # Compare with the expected output
  if diff -u "$tmp_output" "$expected" >/dev/null; then
    echo "✅ Test passed."
    ((passed++))
  else
    echo "❌ Test failed. Differences:"
    diff -u "$tmp_output" "$expected"
    ((failed++))
  fi
  echo
  
  # Remove the temp output
  rm -f "$tmp_output"
done

###############################################################################
# SUMMARY
###############################################################################
echo "=================================================="
echo "Test Summary:"
echo "Passed: $passed"
echo "Failed: $failed"
echo "=================================================="

# Exit with non-zero status if any test failed
if [ "$failed" -gt 0 ]; then
  exit 1
fi
exit 0
