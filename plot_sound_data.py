import matplotlib.pyplot as plt

# Read the data from the file
data = []
with open('sound_data.csv', 'r') as file:
    for line in file:
        line = line.strip().split(',')
        data.append([float(line[0]), float(line[1])])
# Separate the x and y values
x = [i for i, _ in enumerate(data)]
y1 = [row[0] for row in data]
y2 = [row[1] for row in data]
# Plot the data
plt.plot(x, y1, label='Left')
plt.plot(x, y2, label='Right')
# Add labels and legend
plt.xlabel('Line Number')
plt.ylabel('Value')
plt.legend()
# Show the plot
plt.show()

