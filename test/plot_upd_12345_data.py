import socket

import matplotlib.pyplot as plt

NUM = 500

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to a specific IP address and port
server_address = ('', 12345)
sock.bind(server_address)

# Collect NUM pairs of ASCII numbers
pairs = []
for _ in range(NUM):
    data, _ = sock.recvfrom(1024)
    pair_strings = data.decode().strip().split('\r\n')
    for pair_string in pair_strings:
        if pair_string and ',' in pair_string:
            numbers = pair_string.split(',')
            pairs.append((int(numbers[0]), int(numbers[1])))
            if len(pairs) >= NUM:
                break
    if len(pairs) >= NUM:
        break
print(f'Pairs: {pairs}')


# Disconnect from the UDP port
sock.close()

# Plot the numbers on a graph
x = range(1, NUM+1)
y1 = [pair[0] for pair in pairs]
y2 = [pair[1] for pair in pairs]
print(x, y1, y2)
plt.plot(x, y1, label='Number 1')
plt.plot(x, y2, label='Number 2')
plt.xlabel('Line Number')
plt.ylabel('Number')
plt.legend()
plt.show()