#!/bin/bash
#
# run_tests.sh
# This script compiles artetris.c and runs several test cases.
# Each test case specifies board dimensions and a sequence of tetromino letters.
# The tetromino sequences are chosen so that the total active area of the pieces
# (each piece covers an area of 4) exactly matches the board area.
#
# Tetromino letters can be one of: I, O, T, Z, S, L, J.
# Complete tilings (perfect or complete solutions) exist only when the pieces
# exactly tile the board with no overlaps or gaps.
#

# Compile artetris.c
echo "Compiling artetris.c..."
gcc -std=c89 -pedantic -Wall -Wextra -Werror artetris.c -o artetris
if [ $? -ne 0 ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi
echo "Compilation successful."
echo ""

# Define test cases as "rows cols pieces|Description"
# Note: For a board of area A and tetromino area 4, the sequence must have exactly A/4 letters.
tests=(
"1 4 I|Case 1: Minimal board 1x4 with piece I (area 4)"
"2 2 O|Case 2: Simple square board 2x2 with piece O (area 4)"
"3 3 T|Case 3: Mismatch board 3x3 with piece T (active area 4 vs board area 9)"
"4 4 IOTZ|Case 4: 4x4 board with pieces I, O, T, Z (area 16)"
"3 4 IOT|Case 5: 3x4 board with pieces I, O, T (area 12)"
"2 3 S|Case 6: 2x3 board with piece S (active area 4 vs board area 6)"
"4 4 JLSO|Case 7: 4x4 board with pieces J, L, S, O (area 16)"
)

# Loop over test cases
for test in "${tests[@]}"; do
    IFS='|' read -r config description <<< "$test"
    echo "$description"
    IFS=' ' read -r rows cols pieces <<< "$config"
    ./artetris "$rows" "$cols" "$pieces"
    echo ""
done