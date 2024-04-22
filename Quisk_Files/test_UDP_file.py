import socket
import struct
import wave
import numpy as np
import cProfile

# Constants
PORT = 12345
PACKET_SIZE = 1468 # 4 bytes for packet number + 183 * 8 bytes for int32_t pairs
SAMPLE_RATE = 96000 # 96 ks/s
CHANNELS = 2 # Stereo

def main():
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
        audio_data = struct.unpack('<366i', data[4:]) # Little endian

        # Pair up the integers as left and right audio samples
        audio_data_pairs = [(audio_data[i+1], audio_data[i]) for i in range(0, len(audio_data), 2)]
        packets.append((packet_number, audio_data_pairs))
    # Sort packets by packet number
    packets.sort(key=lambda x: x[0])

    # Prepare the audio data for writing to a .wav file
    # Since the audio data is now a list of tuples, we need to flatten it
    audio_data_sorted = np.array([item for sublist in [item[1] for item in packets] for item in sublist], dtype=np.int32)

    # Write to a .wav file
    # print(audio_data_pairs)
    with wave.open('output.wav', 'wb') as wav_file:
        wav_file.setnchannels(CHANNELS)
        wav_file.setsampwidth(4) # Assuming 16-bit samples
        wav_file.setframerate(SAMPLE_RATE)
        wav_file.writeframes(audio_data_sorted.tobytes())

    print("Audio data written to output.wav")

if __name__ == "__main__":
    cProfile.run('main()')