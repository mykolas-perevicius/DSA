import subprocess
import random
import os

def generate_stress_grid(rows, cols, allowed_shapes, filename, difficulty=0.5):
    """Generates a stress-test grid with strategically placed blocked cells."""
    grid = [['-' for _ in range(cols)] for _ in range(rows)]
    total_area = rows * cols
    placed_shapes = []
    shapes_area = 0

    available_shapes = list(allowed_shapes)
    while shapes_area < total_area and available_shapes:
        shape_char = random.choice(available_shapes)
        placed = False
        # Try to place in a few random locations
        for _ in range(5):  # Limited placement attempts
            r = random.randint(0, rows - 1)
            c = random.randint(0, cols - 1)
            if can_place_in_python(grid, shape_char, r, c):
                place_in_python(grid, shape_char, r, c)
                placed_shapes.append(shape_char)
                shapes_area += get_shape_area(shape_char)
                placed = True
                break
        if not placed:
            available_shapes.pop(0)

    # --- Strategic Blocking ---
    remaining_area = total_area - shapes_area
    empty_cells = [(r, c) for r in range(rows) for c in range(cols) if grid[r][c] == '-']
    random.shuffle(empty_cells)

    # Block cells based on difficulty:
    num_to_block = min(int(remaining_area * difficulty), len(empty_cells))
    for i in range(num_to_block):
      r,c = empty_cells[i]
      grid[r][c] = '#'
    
    # Fill rest with random blocks:
    shapes_area = 0
    for r in range(rows):
      for c in range(cols):
        if(grid[r][c] != '#'): shapes_area+=1
    remaining_area = total_area - shapes_area

    empty_cells = [(r, c) for r in range(rows) for c in range(cols) if grid[r][c] == '-']
    random.shuffle(empty_cells)

    # Block cells based on difficulty:
    num_to_block = min(remaining_area, len(empty_cells))
    for i in range(num_to_block):
      r,c = empty_cells[i]
      grid[r][c] = '#'
    
    with open(filename, "w") as f:
        f.write("".join(placed_shapes) + "\n")
        for row in grid:
            f.write("".join(row) + "\n")
    return True


def get_shape_area(shape_char):
    """Gets shape area (DUPLICATED LOGIC)."""
    templates = {
        'A': 5, 'C': 7, 'D': 7, 'F': 7,
        'I': 7, 'J': 8, 'K': 5, 'L': 8,
        'M': 8, 'N': 7, 'O': 7, 'Q': 8
    }
    return templates.get(shape_char, 0)

def can_place_in_python(grid, shape_char, row, col):
    """Checks shape placement (DUPLICATED LOGIC)."""
    shape_data = get_shape_data(shape_char)
    if not shape_data: return False
    rows, cols, data = shape_data
    if row + rows > len(grid) or col + cols > len(grid[0]): return False
    for r in range(rows):
        for c in range(cols):
            if data[r][c] != '.' and grid[row + r][col + c] != '-': return False
    return True

def place_in_python(grid, shape_char, row, col):
    """Places shape (DUPLICATED LOGIC)."""
    shape_data = get_shape_data(shape_char)
    if not shape_data: return
    rows, cols, data = shape_data
    for r in range(rows):
        for c in range(cols):
            if data[r][c] != '.': grid[row + r][col + c] = data[r][c]

def get_shape_data(shape_char):
    """Shape data (DUPLICATED LOGIC)."""
    templates = {
        'A': (5, 1, ["A", "A", "A", "A", "A"]),
        'C': (3, 3, ["CC.", ".CC", ".C."]),
        'D': (4, 2, [".D", ".D", ".D", "DD"]),
        'F': (3, 2, ["FF", "FF", ".F"]),
        'I': (4, 2, ["I.", "I.", "II", ".I"]),
        'J': (3, 3, ["JJJ", ".J.", ".J."]),
        'K': (2, 3, ["K.K", "KKK"]),
        'L': (3, 3, ["..L", "..L", "LLL"]),
        'M': (3, 3, ["..M", ".MM", "MM."]),
        'N': (3, 3, [".N.", "NNN", ".N."]),
        'O': (4, 2, ["O.", "OO", "O.", "O."]),
        'Q': (3, 3, [".QQ", ".Q.", "QQ."]),
    }
    return templates.get(shape_char)

def run_artetris(grid_file, timeout_sec=10):  # Longer timeout for stress tests
    """Runs artetris, capturing output."""
    try:
        result = subprocess.run(
            ["./artetris", grid_file],
            capture_output=True,
            text=True,
            timeout=timeout_sec,
            check=False,
        )
        return result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired:
        return "Timeout", "", -1

def stress_test(num_tests=100, max_rows=20, max_cols=20):
    """Runs the stress tests."""
    compile_result = subprocess.run(
        ["gcc", "-Wall", "-ansi", "-Wextra", "-Werror", "artetris.c", "-o", "artetris"],
        capture_output=True, text=True)
    if compile_result.returncode != 0:
        raise RuntimeError(f"C compilation failed:\n{compile_result.stderr}")

    shapes = "ACDFIJKLMN"
    results = {
        "solver_pass": 0,
        "solver_fail_no_solution": 0,
        "solver_fail_parsing": 0,
        "solver_fail_other": 0,
        "timeout": 0,
    }
    total_tests = 0

    for i in range(num_tests):
        rows = random.randint(5, max_rows)  # Start with slightly larger grids
        cols = random.randint(5, max_cols)
        filename = f"stress_test_grid_{i}.txt"
        difficulty = random.uniform(0.2, 0.8)  # Vary the difficulty
        num_shapes_to_use = random.randint(1, min(6, len(shapes))) #more shapes
        selected_shapes = "".join(random.sample(list(shapes), num_shapes_to_use))

        if generate_stress_grid(rows, cols, selected_shapes, filename, difficulty):
            total_tests += 1
            stdout, stderr, return_code = run_artetris(filename, timeout_sec=10)

            if return_code == 0:
                results["solver_pass"] += 1
                print(f"Test {i}: PASS (Grid: {rows}x{cols}, Shapes: {selected_shapes}, Difficulty: {difficulty:.2f})")
            elif return_code == -1 and "Timeout" in stdout:
                results["timeout"] += 1
                print(f"Test {i}: TIMEOUT (Grid: {rows}x{cols}, Shapes: {selected_shapes}, Difficulty: {difficulty:.2f})")
            elif "Error:" in stdout or "Error:" in stderr:
                if "No solution found" in stdout:
                    results["solver_fail_no_solution"] += 1
                    print(f"Test {i}: FAIL (No Solution, Grid: {rows}x{cols}, Shapes: {selected_shapes}, Difficulty: {difficulty:.2f})")
                elif "Error reading grid file" in stdout or "Error reading grid file" in stderr:
                    results["solver_fail_parsing"] += 1
                    print(f"Test {i}: FAIL (Parsing, Grid: {rows}x{cols}, Shapes: {selected_shapes}, Difficulty: {difficulty:.2f})")
                else:
                    results["solver_fail_other"] += 1
                    print(f"Test {i}: FAIL (Other, Grid: {rows}x{cols}, Shapes: {selected_shapes}, Difficulty: {difficulty:.2f})")
                    print(f"Stdout:\n{stdout}\nStderr:\n{stderr}")

            else:
                results["solver_fail_other"] += 1
                print(f"Test {i}: FAIL (Other, Grid: {rows}x{cols}, Shapes: {selected_shapes}, Difficulty: {difficulty:.2f})")
                print(f"  Stdout:\n{stdout}\nStderr:\n{stderr}")
        else:
          total_tests += 1
          results["solver_fail_other"] += 1 #shouldnt fail but just in case
          print(f"Test {i}: FAIL (Grid Gen, Grid: {rows}x{cols}, Shapes: {selected_shapes}, Difficulty: {difficulty:.2f})")
          print(f"Stdout:\n{stdout}\nStderr:\n{stderr}")

        if os.path.exists(filename):
            os.remove(filename)

    print("\n--- Stress Test Summary ---")
    print(f"Total Tests: {total_tests}")
    print(f"Solver - Passed: {results['solver_pass']}/{total_tests}")
    print(f"Solver - Failed (No Solution): {results['solver_fail_no_solution']}/{total_tests}")
    print(f"Solver - Failed (Parsing): {results['solver_fail_parsing']}/{total_tests}")
    print(f"Solver - Failed (Other): {results['solver_fail_other']}/{total_tests}")
    print(f"Solver - Timeouts: {results['timeout']}/{total_tests}")

if __name__ == "__main__":
    stress_test(num_tests=1000, max_rows=20, max_cols=20)