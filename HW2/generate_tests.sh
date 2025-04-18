#!/bin/bash

# generate_tests.sh
# This script generates test cases for the artetris tiling solver, based on a given piece sequence.
# It computes the total active area, enumerates board dimensions with matching area, and then runs the artetris solver.
# If a valid tiling is found (i.e. complete solution exists with at least a given number of solutions),
# it prints out the board configuration and solution summary.

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <piece_sequence> [min_solutions]"
    echo "Or run with 'all' to process a predefined candidate list."
    exit 1
fi

if [ "$1" = "all" ]; then
    # Generate candidate sequences using each allowed shape at most once (all subsets of ALLOWED with length >= 3)
    ALLOWED=( A C D F I J K L M N O Q)
    
    generate_combinations() {
        local start=$1
        local current=$2
        local n=${#ALLOWED[@]}
        if [ $start -ge $n ]; then
            if [ ${#current} -ge 2 ]; then
                echo "$current"
            fi
            return
        fi
        # Option to include ALLOWED[start]
        generate_combinations $(( start + 1 )) "$current${ALLOWED[start]}"
        # Option to exclude ALLOWED[start]
        generate_combinations $(( start + 1 )) "$current"
    }
    
    candidates=( $(generate_combinations 0 "") )
    
    # Define area mapping for each shape letter
    declare -A area
    area[A]=6
    area[C]=6
    area[D]=6
    area[F]=6
    area[I]=4
    area[J]=3
    area[K]=6
    area[L]=6
    area[M]=6
    area[N]=6
    area[O]=4
    area[Q]=6
    
    # For each candidate sequence, compute total area and generate all board rectangle dimensions
    for candidate in "${candidates[@]}"; do
        total=0
        len=${#candidate}
        for (( i=0; i<len; i++ )); do
            letter="${candidate:$i:1}"
            total=$(( total + area[$letter] ))
        done
        echo "Candidate sequence: $candidate, total area = $total"
        for (( r=1; r<=total; r++ )); do
            if [ $(( total % r )) -eq 0 ]; then
                c=$(( total / r ))
                echo "Test case: ${r}x${c} $candidate"
            fi
        done
        echo "-------------------------------------"
    done
    
    echo "Proceeding to test candidate configurations..."
else
    candidates=( "$1" )
fi

# Optional second argument: minimum number of solutions required; default to 1 if not provided
if [ -z "$2" ]; then
    min_solutions=1
else
    min_solutions=$2
fi

declare -A testSet
full_valid_count=0

for piece_seq in "${candidates[@]}"; do
    total_pieces=${#piece_seq}
    declare -A area
    area[A]=6
    area[C]=6
    area[D]=6
    area[F]=6
    area[I]=4
    area[J]=3
    area[K]=6
    area[L]=6
    area[M]=6
    area[N]=6
    area[O]=4
    area[Q]=6
    active_area=0
    for (( i=0; i<${#piece_seq}; i++ )); do
         letter=${piece_seq:$i:1}
         active_area=$(( active_area + area[$letter] ))
    done
    echo "=============================================="
    echo "Processing candidate piece sequence: $piece_seq"
    echo "Total pieces: $total_pieces"
    echo "Active area (must equal board area): $active_area"
    echo "Minimum solutions threshold: ${min_solutions:-1}"
    best_sol_count=-1
    best_r=0
    best_c=0
    valid_count=0

    # Function to run artetris for given rows and columns and capture output
    run_artetris() {
        local rows=$1
        local cols=$2
        local start end elapsed
        start=$(date +%s.%N)
        # Run the solver with a timeout of 30 seconds and capture output
        output=$(timeout 30 ./artetris "$rows" "$cols" "$piece_seq" 2>&1)
        end=$(date +%s.%N)
        elapsed=$(echo "$end - $start" | bc)
        echo "$output"
        echo "Time: $elapsed seconds"
    }

    # Function to extract number of solutions from artetris output
    extract_solution_count() {
        echo "$1" | grep "Found" | head -n1 | awk '{print $2}'
    }

    max_dim=$(( active_area * 2 ))
    for (( r=1; r<=max_dim; r++ )); do
      for (( c=1; c<=max_dim; c++ )); do
         if [ $(( r * c )) -eq "$active_area" ]; then
             # Skip boring boards where either dimension is 1
             if [ "$r" -eq 1 -o "$c" -eq 1 ]; then
                 echo "Skipping boring board: ${r}x${c}"
                 continue
             fi
             echo "-------------------------------------------"
             echo "Testing board: ${r}x${c} (area=$(( r*c )))"
             result=$(run_artetris "$r" "$c")
             sol_count=$(extract_solution_count "$result")
             if [ -z "$sol_count" ]; then
                 sol_count=0
             fi
             if [ "$sol_count" -gt "$best_sol_count" ]; then
                 best_sol_count=$sol_count
                 best_r=$r
                 best_c=$c
             fi
             if [ "$sol_count" -ge "${min_solutions:-1}" ]; then
                 echo "VALID tiling found for ${r}x${c} board! (Solutions: $sol_count)"
                 valid_count=$(( valid_count + 1 ))
                 echo "Tiling output (partial):"
                 echo "$result" | grep -A 10 "Best Solution:"
                 testCase="${r} ${c} ${piece_seq}"
                 if [ -z "${testSet[$testCase]}" ]; then
                     testSet["$testCase"]=1
                     echo "$testCase" >> generated_tests.txt
                 fi
             else
                 echo "No complete tiling meeting threshold for ${r}x${c} board. (Solutions: $sol_count)"
             fi
             echo "-------------------------------------------"
             echo ""
         fi
      done
    done
    if [ "$valid_count" -eq 0 ]; then
        if [ "$best_r" -gt 0 ]; then
            echo "Recording board configuration for sequence $piece_seq: ${best_r}x${best_c} with best solution count $best_sol_count"
            testCase="${best_r} ${best_c} ${piece_seq}"
            if [ -z "${testSet[$testCase]}" ]; then
                testSet["$testCase"]=1
                echo "$testCase" >> generated_tests.txt
            fi
        else
            echo "No board configuration found for candidate sequence $piece_seq"
        fi
    fi
    echo "Total valid board configurations for sequence $piece_seq found: $valid_count"
    echo "=============================================="
    echo ""
    full_valid_count=$(( full_valid_count + valid_count ))
done
echo "Grand total valid board configurations found: $full_valid_count"
echo "Generated test cases saved in generated_tests.txt" 