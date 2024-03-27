filename = './sound_data.csv'
line_number = 0
last_integer = None
incorrect_lines = []

with open(filename, 'r') as file:
    for line in file:
        line_number += 1
        integer = int(line.strip())
        
        if last_integer is not None and integer != last_integer + 1:
            incorrect_lines.append(line_number)
        
        last_integer = integer

print("Lines with incorrect pattern:", incorrect_lines)

