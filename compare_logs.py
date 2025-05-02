import re

expected = "roms/tests/nestest.log"
actual = "roms/tests/nestest_output.log"

def compare_logs(expected, actual):
    with open(expected) as e, open(actual) as a:
        for i, (expected_line, actual_line) in enumerate(zip(e, a), 1):
            if expected_line.strip() != actual_line.strip():
                print(f"Line {i} differs:")
                print(f"Expected: {expected_line.strip()}")
                print(f"Actual:   {actual_line.strip()}")
                return

def simplify_log(filename):
    simplified_lines = []

    # This regex matches the line's structure
    pattern = re.compile(
        r"^(?P<pc>[A-F0-9]{4}).*?A:(?P<A>[A-F0-9]{2}) X:(?P<X>[A-F0-9]{2}) Y:(?P<Y>[A-F0-9]{2}) P:(?P<P>[A-F0-9]{2}) SP:(?P<S>[A-F0-9]{2})"
    )

    with open(filename) as f:
        for line in f:
            match = pattern.search(line)
            if match:
                pc = match.group("pc")
                A = match.group("A")
                X = match.group("X")
                Y = match.group("Y")
                P = match.group("P")
                S = match.group("S")
                simplified_line = f"{pc} A:{A} X:{X} Y:{Y} P:{P} S:{S}"
                simplified_lines.append(simplified_line)

    with open(filename, "w") as f:
        for line in simplified_lines:
            f.write(line + "\n")

# simplify_log(expected)
compare_logs(expected, actual)