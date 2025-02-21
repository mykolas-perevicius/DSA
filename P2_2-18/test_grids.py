import subprocess
import random
import os

def generate_simple_grid(rows, cols, allowed_shapes, filename):
    """Generates a simple grid with a known solution."""
    grid = [['-' for _ in range(cols)] for _ in range(rows)]
    total_area = rows * cols
    shapes_area = 0
    placed_shapes = []
    available_shapes = list(allowed_shapes)
    random.shuffle(available_shapes)

    # Simple placement, shape by shape until full or no more shapes
    while shapes_area < total_area and available_shapes:
        shape_char = available_shapes.pop(0)  # Use shapes in a shuffled order
        placed = False
        for r in range(rows):
            for c in range(cols):
                if can_place_in_python(grid, shape_char, r, c):
                    place_in_python(grid, shape_char, r, c)
                    placed_shapes.append(shape_char)
                    shapes_area += get_shape_area(shape_char)
                    placed = True
                    break  # Move to the next shape after placing one
            if placed:
                break
        if not placed: # No more placements for the current shape
          continue

    if(shapes_area != total_area): return False

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

def run_artetris(grid_file, timeout_sec=5):
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


def simple_grid_test(num_tests=100, max_rows=8, max_cols=8):
    """Runs basic grid generation and solver tests."""
    compile_result = subprocess.run(
        ["gcc", "-Wall", "-ansi", "-Wextra", "-Werror", "artetris.c", "-o", "artetris"],
        capture_output=True, text=True)
    if compile_result.returncode != 0:
        raise RuntimeError(f"C compilation failed:\n{compile_result.stderr}")

    shapes = "ACDFIJKLMN"
    results = {
        "generation_pass": 0,
        "generation_fail": 0,
        "solver_pass": 0,
        "solver_fail_no_solution": 0,
        "solver_fail_parsing": 0,
        "solver_fail_other": 0,
        "timeout": 0,
    }

    for i in range(num_tests):
        rows = random.randint(2, max_rows)
        cols = random.randint(2, max_cols)
        filename = f"test_grid_{i}.txt"
        num_shapes_to_use = random.randint(1, min(4, len(shapes)))  # Use a few shapes
        selected_shapes = "".join(random.sample(list(shapes), num_shapes_to_use))

        if generate_simple_grid(rows, cols, selected_shapes, filename):
          results["generation_pass"] += 1
          stdout, stderr, return_code = run_artetris(filename, timeout_sec=2)  # Shorter timeout

          if return_code == 0:
              results["solver_pass"] += 1
              print(f"Test {i}: PASS (Grid: {rows}x{cols}, Shapes: {selected_shapes})")
          elif return_code == -1 and "Timeout" in stdout:
              results["timeout"] += 1
              print(f"Test {i}: TIMEOUT (Grid: {rows}x{cols}, Shapes: {selected_shapes})")
          elif "Error:" in stdout or "Error:" in stderr:
              if "No solution found" in stdout:
                  results["solver_fail_no_solution"] += 1
                  print(f"Test {i}: FAIL (No Solution, Grid: {rows}x{cols}, Shapes: {selected_shapes})")
              elif "Error reading grid file" in stdout or "Error reading grid file" in stderr:
                  results["solver_fail_parsing"] += 1
                  print(f"Test {i}: FAIL (Parsing, Grid: {rows}x{cols}, Shapes: {selected_shapes})")
              else:
                  results["solver_fail_other"] += 1
                  print(f"Test {i}: FAIL (Other, Grid: {rows}x{cols}, Shapes: {selected_shapes})")
                  print(f"Stdout:\n{stdout}\nStderr:\n{stderr}")
          else:
              results["solver_fail_other"] += 1
              print(f"Test {i}: FAIL (Other, Grid: {rows}x{cols}, Shapes: {selected_shapes})")
              print(f"Stdout:\n{stdout}\nStderr:\n{stderr}")

        else:
          results["generation_fail"] += 1
          print(f"Test {i}: Generation FAIL (Grid: {rows}x{cols}, Shapes: {selected_shapes})")


        if os.path.exists(filename):
            os.remove(filename)


    print("\n--- Simple Test Summary ---")
    print(f"Total Tests: {num_tests}")
    print(f"Grid Generation - Passed: {results['generation_pass']}/{num_tests}")
    print(f"Grid Generation - Failed: {results['generation_fail']}/{num_tests}")
    print(f"Solver - Passed: {results['solver_pass']}/{results['generation_pass']}")
    print(f"Solver - Failed (No Solution): {results['solver_fail_no_solution']}/{results['generation_pass']}")
    print(f"Solver - Failed (Parsing): {results['solver_fail_parsing']}/{results['generation_pass']}")
    print(f"Solver - Failed (Other): {results['solver_fail_other']}/{results['generation_pass']}")
    print(f"Solver - Timeouts: {results['timeout']}/{results['generation_pass']}")


if __name__ == "__main__":
    simple_grid_test()