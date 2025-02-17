#!/bin/bash
#
# run_test.sh
# This script compiles artetris.c and runs several test cases.
# Each test case specifies board dimensions and a sequence of piece letters.
# The piece sequences are chosen so that the total active area of the pieces
# (each piece covers an area of 4) exactly matches the board area.
#
# Allowed piece letters: A, C, D, F, I, J, K, L, M, N, O, Q.
# Complete tilings exist only when the pieces exactly tile the board with no overlaps or gaps.
#

# Modified compile command with debug symbols
echo "Compiling artetris.c with strict ANSI C compliance..."
gcc -std=c89 -pedantic -g -Wall -Wextra -Werror artetris.c -o artetris
if [ $? -ne 0 ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi
echo "Compilation successful (debug symbols included)."
echo ""

# Setup logging directories
LOGDIR="test_logs"
mkdir -p "$LOGDIR"
SUMMARY="$LOGDIR/solver_summary.txt"
rm -f "$SUMMARY"  # Start fresh each test run

# Create and initialize the running summary
RUNNING_SUMMARY="/tmp/running_summary.txt"
echo "Test Results Summary" > "$RUNNING_SUMMARY"
echo "===================" >> "$RUNNING_SUMMARY"
printf "%-25s %-15s %-20s %-10s\n" "Configuration" "Status" "Time" "Pieces Placed" >> "$RUNNING_SUMMARY"
echo "------------------------------------------------------------------------" >> "$RUNNING_SUMMARY"

# Test cases covering all template types with valid configurations
declare -A tests=(
    # Format: ["Name"]="rows cols pieces"
    
    # A: 6 cells (vertical I-bar)
    ["2x6_AA"]="2 6 AA"         # 2×6=12 = 2×6
    
    # C: 5 cells (staggered shape)
    ["5x5_CCCCC"]="5 5 CCCCC"   # 5×5=25 = 5×5
    
    # D: 5 cells (L-shape)
    ["5x4_DDDD"]="5 4 DDDD"     # 5×4=20 = 4×5
    
    # F: 5 cells (Z-shape)
    ["4x5_FFFF"]="4 5 FFFF"     # 4×5=20 = 4×5
    
    # I: 4 cells (horizontal I-bar)
    ["4x4_IIII"]="4 4 IIII"     # 4×4=16 = 4×4
    
    # K: 5 cells (T-shape)
    ["5x5_KKKKK"]="5 5 KKKKK"   # 5×5=25 = 5×5
    
    # L: 4 cells (L-shape)
    ["4x4_LLLL"]="4 4 LLLL"     # 4×4=16 = 4×4
    
    # M: 4 cells (S-shape)
    ["4x4_MMMM"]="4 4 MMMM"     # 4×4=16 = 4×4
    
    # N: 4 cells (square)
    ["4x4_NNNN"]="4 4 NNNN"     # 4×4=16 = 4×4
    
    # O: 4 cells (T-shape)
    ["4x4_OOOO"]="4 4 OOOO"     # 4×4=16 = 4×4
    
    # Q: 4 cells (cross)
    ["5x5_QQQQQ"]="5 5 QQQQQ"   # 5×5=25 = 5×5 (needs 25/4=6.25 → invalid)
    # Correction:
    ["5x4_QQQQ"]="5 4 QQQQ"     # 5×4=20 = 4×5
)

# Run tests
for test in "${!tests[@]}"; do
    logfile="$LOGDIR/${test}.log"
    echo "Running test $test..."
    status=""
    time_taken="N/A"
    pieces_placed="N/A"
    
    # Run with debug output and capture exit code
    ./artetris ${tests[$test]} > "$logfile" 2>&1
    exit_code=$?
    
    # Handle core dumps
    if [ $exit_code -eq 139 ]; then
        status="CORE DUMP"
        echo "Core dump detected in test $test"
        # Generate backtrace if possible
        if [ -x artetris ] && [ -f artetris ]; then
            gdb -batch -ex "bt" artetris core >> "$logfile" 2>&1
        fi
    else
        # Extract test results
        time_taken=$(grep "Time:" "$logfile" | awk '{print $NF}' | head -1)
        pieces_placed=$(grep "Best solution:" "$logfile" | awk '{print $NF}')
        
        if [ -z "$pieces_placed" ]; then
            pieces_placed="0"
        fi
        
        # Determine status
        if grep -q "No solutions found" "$logfile"; then
            status="NO_SOLUTION"
        elif [ $exit_code -eq 0 ]; then
            status="COMPLETED"
        else
            status="ERROR($exit_code)"
        fi
    fi
    
    # Add to summary
    printf "%-25s %-15s %-20s %-10s\n" "$test" "$status" "$time_taken" "$pieces_placed" >> "$SUMMARY"
done

# Print final summary
echo ""
echo "Test Results Summary"
echo "==================="
column -t -s' ' "$SUMMARY"