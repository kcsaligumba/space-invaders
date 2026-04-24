import sys

if len(sys.argv) != 3:
    print("Usage: python sprite_to_mem.py input.txt output.mem")
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]

rows = []

with open(input_file, "r") as f:
    for line_num, line in enumerate(f, start=1):
        line = line.strip()

        if not line:
            continue

        bits = ""

        for ch in line:
            if ch == "X":
                bits += "1"
            elif ch == ".":
                bits += "0"
            else:
                raise ValueError(f"Invalid character '{ch}' on line {line_num}")

        value = int(bits, 2)
        hex_width = (len(bits) + 3) // 4
        rows.append(f"{value:0{hex_width}X}")

with open(output_file, "w") as f:
    for row in rows:
        f.write(row + "\n")