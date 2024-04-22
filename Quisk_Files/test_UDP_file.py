import socket
import struct
import wave

UDP_IP = "0.0.0.0"
UDP_PORT = 12345

PACKET_SIZE = 1468
PACKET_HEADER_SIZE = 4
DATA_PAIR_SIZE = 8
NUM_DATA_PAIRS = 183

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

# Initialize variables
expected_packet_number = 0
audio_data = []

while expected_packet_number < 4000:
    # Receive UDP packet
    packet, addr = sock.recvfrom(PACKET_SIZE)

    # Extract packet number
    packet_number = struct.unpack('!I', packet[:PACKET_HEADER_SIZE])[0]

    # Check if packet is in order
    if packet_number == expected_packet_number:
        # Extract audio data pairs
        print(packet[PACKET_HEADER_SIZE:])
        data_pairs = struct.unpack('<' + 'ii' * NUM_DATA_PAIRS, packet[PACKET_HEADER_SIZE:])
        print(data_pairs)
        # Append audio data pairs to the list
        audio_data.extend(data_pairs)

    # Increment expected packet number
    expected_packet_number += 1

print (expected_packet_number)
print (len(audio_data))
# Save audio data to a stereo .wav file
with wave.open('output.wav', 'wb') as wav_file:
    wav_file.setnchannels(2)  # Stereo
    wav_file.setsampwidth(4)  # 4 bytes per sample (int32_t)
    wav_file.setframerate(44100)  # Sample rate of 44100 Hz
    wav_file.writeframes(struct.pack('!' + 'ii' * len(audio_data), *audio_data))