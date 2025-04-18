#!/bin/bash

# filter_tests.sh
# This script parses the generated test logs in the test_logs directory and filters out those tests
# that did not yield a valid solution (i.e. contain either 'No valid tiling found for this configuration' or 'No solutions found').
# It then removes the log files and deletes the corresponding entry from generated_tests.txt, so they are not generated again.

LOGDIR="test_logs"
TESTFILE="generated_tests.txt"
TEMPFILE=$(mktemp)

echo "Filtering tests with no valid solutions..."

while IFS= read -r line; do
    # Each line in generated_tests.txt is in the format: <rows> <cols> <piece_sequence>
    r=$(echo "$line" | awk '{print $1}')
    c=$(echo "$line" | awk '{print $2}')
    seq=$(echo "$line" | awk '{print $3}')
    logfile="$LOGDIR/generated_${r}x${c}_${seq}.log"
    
    if [ -f "$logfile" ]; then
        # Check if the log contains either 'No valid tiling found for this configuration' or 'No solutions found' (case insensitive)
        if grep -Ei "No valid tiling found for this configuration|No solutions found" "$logfile"; then
            echo "Removing test: $line (logfile: $logfile) due to no valid solution."
            rm -f "$logfile"
            continue
        fi
    fi
    echo "$line" >> "$TEMPFILE"

done < "$TESTFILE"

mv "$TEMPFILE" "$TESTFILE"

echo "Filtering complete. Remaining tests in $TESTFILE:"
cat "$TESTFILE" 