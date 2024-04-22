import socket
import struct
import wave
import numpy as np

# Constants
PORT = 12345
PACKET_SIZE = 1468 # 4 bytes for packet number + 183 * 8 bytes for int32_t pairs
SAMPLE_RATE = 96000 # 96 ks/s
CHANNELS = 2 # Stereo

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', PORT))

# Prepare to receive packets
packets = []

print("Listening for packets on port 12345...")

while len(packets) < 4000:
    data, addr = sock.recvfrom(PACKET_SIZE)
    packet_number = struct.unpack('<I', data[:4])[0] # Little endian
    audio_data = struct.unpack('<183i', data[4:]) # Little endian
    packets.append((packet_number, audio_data))

# Sort packets by packet number
packets.sort(key=lambda x: x[0])

# Prepare the audio data for writing to a .wav file
audio_data_sorted = np.array([item[1] for item in packets], dtype=np.int32)
audio_data_sorted = audio_data_sorted.flatten()

# Write to a .wav file
with wave.open('output.wav', 'wb') as wav_file:
    wav_file.setnchannels(CHANNELS)
    wav_file.setsampwidth(4) # Assuming 32-bit samples
    wav_file.setframerate(SAMPLE_RATE)
    wav_file.writeframes(audio_data_sorted.tobytes())

print("Audio data written to output.wav")
