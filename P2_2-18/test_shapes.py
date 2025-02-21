def compare_shape_files(generated_file_path, gold_file_path):
    """
    Compares two shape output files line by line and reports differences.

    Args:
        generated_file_path (str): Path to the newly generated shapes.txt file.
        gold_file_path (str): Path to the gold standard shapes_gold.txt file.

    Returns:
        bool: True if files are identical, False otherwise.
    """
    try:
        with open(generated_file_path, 'r') as generated_file, open(gold_file_path, 'r') as gold_file:
            line_num = 1
            files_are_identical = True
            while True:
                generated_line = generated_file.readline()
                gold_line = gold_file.readline()

                if not generated_line and not gold_line:
                    break  # Both files reached EOF

                if generated_line != gold_line:
                    files_are_identical = False
                    print(f"Difference at line {line_num}:")
                    print(f"  Generated: {generated_line.rstrip()}")
                    print(f"  Gold     : {gold_line.rstrip()}")
                    return False # Early exit on first difference

                line_num += 1
            return files_are_identical

    except FileNotFoundError:
        print("Error: One or both of the shape files not found.")
        return False

if __name__ == "__main__":
    generated_file = "generated_test_output.txt"  # Path to the output from ./artetris -test
    gold_file = "shapes_gold.txt"     # Path to your shapes_gold.txt

    if compare_shape_files(generated_file, gold_file):
        print(f"Test Passed! {generated_file} is identical to {gold_file}")
    else:
        print(f"Test Failed! {generated_file} differs from {gold_file}")