import matplotlib.pyplot as plt

# Open the file and read the data
with open("debug_iq_samples.txt", "r") as file:
    lines = file.readlines()

# Split the data into I and Q samples
i_samples = [int(line.split()[0]) for line in lines]
q_samples = [int(line.split()[1]) for line in lines]

# Create the plot
plt.figure(figsize=(10, 6))
plt.plot(i_samples, label='I Samples')
plt.plot(q_samples, label='Q Samples')
plt.title('I and Q Samples')
plt.xlabel('Sample Number')
plt.ylabel('Value')
plt.legend()
plt.grid(True)
plt.show()