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
echo "=====================" >> "$RUNNING_SUMMARY"
# Use tab as delimiter
printf "%s\t%s\t%s\t%s\t%s\n" "Configuration" "Status" "Time" "Pieces Placed" "Fail Reason" > "$SUMMARY"
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
    ["5x4_QQQQ"]="5 4 QQQQ"     # 5×4=20 = 4×5

    # Easy Cases
    ["1x6_A"]="1 6 A"
    ["6x1_A"]="6 1 A"
    ["3x6_AAA"]="3 6 AAA"
    ["5x5_JJJJJ"]="5 5 JJJJJ"

    # Medium Cases
    ["6x6_AAAAAA"]="6 6 AAAAAA"
    ["5x6_IIIIII"]="5 6 IIIIII"
    ["6x5_FFFFFF"]="6 5 FFFFFF"

    # Edge Cases
    ["2x2_A"]="2 2 A"
    ["1x1_N"]="1 1 N"
    ["2x3_JJ"]="2 3 JJ"

    # Mixed Cases
    ["7x3_AIII"]="7 3 AIII"
    ["3x7_AIII"]="3 7 AIII"
    ["6x6_ACCCDIJ"]="6 6 ACCCDIJ"
    ["5x4_IIKL"]="5 4 IIKL"
    # Hardest test case: use all allowed pieces in a challenging mix
    ["Hardest_All"]="8 16 AAACCDDFFIIJJKKLLMMNNOOQQ"
    ["6x2_AA"]="6 2 AA"
    ["3x5_NNN"]="3 5 NNN"
)

# Run tests
for test in "${!tests[@]}"; do
    logfile="$LOGDIR/${test}.log"
    echo "Running test $test..."
    status=""
    time_taken="N/A"
    pieces_placed="N/A"
    failure_reason=""
    
    # Run with debug output and capture exit code
    ./artetris ${tests[$test]} > "$logfile" 2>&1
    exit_code=$?
    
    # Handle core dumps
    if [ $exit_code -eq 139 ]; then
        status="CORE DUMP"
        echo "Core dump detected in test $test"
        if [ -x artetris ] && [ -f artetris ]; then
            gdb -batch -ex "bt" artetris core >> "$logfile" 2>&1
        fi
    else
        # Extract test results
        time_taken=$(grep "Time:" "$logfile" | awk '{print $NF}' | head -1)
        pieces_placed=$(grep "solution has" "$logfile" | awk '{print $(NF-1)}')
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
    
    if [ "$status" != "COMPLETED" ]; then
         failure_reason=$(grep -E "Error:|No valid tiling" "$logfile" | head -1)
    fi
    
    # Add to summary (use tab as delimiter)
    printf "%s\t%s\t%s\t%s\t%s\n" "$test" "$status" "$time_taken" "$pieces_placed" "$failure_reason" >> "$SUMMARY"

done

# NEW BLOCK: Run additional tests from generated_tests.txt if it exists
if [ -f generated_tests.txt ]; then
    echo "Running additional tests from generated_tests.txt..."
    while IFS= read -r line; do
        # Parse test parameters: rows, cols, and piece sequence
        r=$(echo "$line" | awk '{print $1}')
        c=$(echo "$line" | awk '{print $2}')
        piece_seq=$(echo "$line" | awk '{print $3}')
        logfile="$LOGDIR/generated_${r}x${c}_${piece_seq}.log"
        echo "Running generated test ${r}x${c} with sequence ${piece_seq}..."
        ./artetris "$r" "$c" "$piece_seq" > "$logfile" 2>&1
        exit_code=$?
        time_taken=$(grep "Time:" "$logfile" | awk '{print $NF}' | head -1)
        pieces_placed=$(grep "solution has" "$logfile" | awk '{print $(NF-1)}')
        if [ -z "$pieces_placed" ]; then
            pieces_placed="0"
        fi
        if grep -q "No solutions found" "$logfile"; then
            status="NO_SOLUTION"
        elif [ $exit_code -eq 0 ]; then
            status="COMPLETED"
        else
            status="ERROR($exit_code)"
        fi
        failure_reason=""
        if [ "$status" != "COMPLETED" ]; then
            failure_reason=$(grep -E "Error:|No valid tiling" "$logfile" | head -1)
        fi
        # Append test result to the SUMMARY
        printf "%s\t%s\t%s\t%s\t%s\n" "Generated_${r}x${c}_${piece_seq}" "$status" "$time_taken" "$pieces_placed" "$failure_reason" >> "$SUMMARY"
    done < generated_tests.txt
fi

# Print final summary
echo ""
echo "Test Results Summary"
echo "====================="
column -t -s $'\t' "$SUMMARY"