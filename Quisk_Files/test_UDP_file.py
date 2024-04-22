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
    # Unpack the stereo audio data into a list of tuples (right, left)
    audio_data = struct.unpack('<183i', data[4:]) # Little endian

    # Assuming audio_data is a bytes object
    audio_data_ints = struct.unpack('<' + 'i' * (len(audio_data) // 4), audio_data)

    # Pair up the integers as left and right audio samples
    audio_data_pairs = [(audio_data_ints[i], audio_data_ints[i+1]) for i in range(0, len(audio_data_ints), 2)]
    audio_data = [(audio_data_pairs[i][1], audio_data_pairs[i][0]) for i in range(len(audio_data_pairs))]
    packets.append((packet_number, audio_data))

# Sort packets by packet number
packets.sort(key=lambda x: x[0])

# Prepare the audio data for writing to a .wav file
# Since the audio data is now a list of tuples, we need to flatten it
audio_data_sorted = np.array([item for sublist in [item[1] for item in packets] for item in sublist], dtype=np.int32)

# Write to a .wav file
with wave.open('output.wav', 'wb') as wav_file:
    wav_file.setnchannels(CHANNELS)
    wav_file.setsampwidth(2) # Assuming 16-bit samples
    wav_file.setframerate(SAMPLE_RATE)
    wav_file.writeframes(audio_data_sorted.tobytes())

print("Audio data written to output.wav")
