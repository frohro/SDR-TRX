import socket
import struct

UDP_IP = "0.0.0.0"
UDP_PORT = 12345

PACKET_SIZE = 1468
PACKET_HEADER_SIZE = 4
NUM_DATA_PAIRS = 183

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

# Initialize variables
expected_packet_number = None
audio_data = []

while True:
    # Receive UDP packet
    packet, addr = sock.recvfrom(PACKET_SIZE)

    # Extract packet number
    packet_number = struct.unpack('!I', packet[:PACKET_HEADER_SIZE])[0]

    # If expected_packet_number is None, this is the first packet
    if expected_packet_number is None:
        expected_packet_number = packet_number

    # Check if packet is in order
    print(packet_number, expected_packet_number)
    if packet_number == expected_packet_number:
        # Extract audio data pairs
        print(packet[PACKET_HEADER_SIZE:])
        data_pairs = struct.unpack('<' + 'ii' * NUM_DATA_PAIRS, packet[PACKET_HEADER_SIZE:])
        print(data_pairs)
        # Append audio data pairs to the list
        audio_data.extend(data_pairs)

    # Increment expected packet number regardless of whether the packet was in order
    expected_packet_number += 1

    # Stop the loop after receiving 4000 packets
    if len(audio_data) // NUM_DATA_PAIRS >= 4000:
        break