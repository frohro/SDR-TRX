import socket
import struct
import matplotlib.pyplot as plt

# Initialize variables to keep track of packet order
expected_packet_number = 0
received_packets = {}
# Create a variable to hold the concatenated data
concatenated_data = b''
# Initialize a counter for out-of-order packets
out_of_order_count = 0

# Create a figure for the plot
fig, ax = plt.subplots()

# Initialize a list to hold the data points
data_points = []

def update(num):
    # Remove the oldest data point if we have 1000 data points
    if len(data_points) == 1000:
        data_points.pop(0)

    # Add the newest data point
    data_points.append(pairs[num])

    # Update the plot
    ax.clear()
    ax.plot(data_points)
    plt.xlabel('Pair Number')
    plt.ylabel('Value')
    plt.title('Plot of Data')

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to a specific IP address and port
server_address = ('localhost', 12345)
sock.bind(server_address)

# Initialize variables to keep track of packet order
expected_packet_number = 0
received_packets = {}
# Create a variable to hold the concatenated data
concatenated_data = b''
# Initialize a counter for out-of-order packets
out_of_order_count = 0

while True:
    data, address = sock.recvfrom(1468)
    packet_number = struct.unpack('<I', data[:4])[0]
    packet_data = data[4:]

    if packet_number == expected_packet_number:
        # Append the packet data to the concatenated data
        concatenated_data += packet_data
        expected_packet_number += 1

        # Process any out-of-order packets that have now become in-order
        while expected_packet_number in received_packets:
            # Append the packet data to the concatenated data
            concatenated_data += received_packets[expected_packet_number]
            del received_packets[expected_packet_number]
            expected_packet_number += 1
    else:
        # Store out-of-order packets to be processed later
        received_packets[packet_number] = packet_data
        # Increment the out-of-order counter
        out_of_order_count += 1

    ani = FuncAnimation(fig, update, frames=range(1000), repeat=False)
    plt.show()
    from matplotlib.animation import FuncAnimation

# After the loop, out_of_order_count will hold the number of out-of-order packets
print(f"Number of out-of-order packets: {out_of_order_count}")
# Unpack the data into pairs
pairs = []
for i in range(0, len(concatenated_data), 8):  # Assuming each number is 4 bytes
    pair = struct.unpack('<II', concatenated_data[i:i+8])
    pairs.append(pair)

# Make sure we only take the first 1000 pairs
pairs = pairs[:1000]

# Separate the pairs into two lists for plotting
x_values = [i for i in range(len(pairs))]
y_values = [pair[0] for pair in pairs]  # Assuming you want to plot the first value of each pair

# Create the plot
plt.plot(x_values, y_values)
plt.xlabel('Pair Number')
plt.ylabel('Value')
plt.title('Plot of Data')
plt.show()