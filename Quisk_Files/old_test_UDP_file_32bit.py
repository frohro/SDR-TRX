import socket
import struct
import wave
import numpy as np
import cProfile

# Constants
PORT = 12345
PACKET_SIZE = 1468 # 4 bytes for packet number + 183 * 8 bytes for int32_t pairs
SAMPLE_RATE = 48000 # 96 ks/s
CHANNELS = 2 # Stereo

def main():
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024*1024)
    sock.bind(('', PORT))

    # Prepare to receive packets
    packets = []

    print("Listening for packets on port 12345...")
    while len(packets) < 10000:
        data, addr = sock.recvfrom(PACKET_SIZE)
        packet_number = struct.unpack('<I', data[:4])[0] # Little endian

        # Unpack the stereo audio data into a list of tuples (right, left)
        audio_data = struct.unpack('<366i', data[4:]) # Little endian

        # Pair up the integers as left and right audio samples
        audio_data_pairs = list(zip(audio_data[1::2], audio_data[::2]))
        packets.append((packet_number, audio_data_pairs))

    # Sort packets by packet number
    packets.sort(key=lambda x: x[0])

    # Check for missing or out-of-order packets
    expected_packet_number = packets[0][0]
    previous_packet_number = None
    missed_packets = 0
    for packet in packets:
        packet_number = packet[0]
        if previous_packet_number is not None:
            if packet_number < previous_packet_number:
                print(f"Packets not sorted correctly. Previous packet number {previous_packet_number}, current packet number {packet_number}")
            elif packet_number != previous_packet_number + 1:
                print(f"Packet missed. Expected packet number {previous_packet_number + 1}, but received packet number {packet_number}")
                missed_packets += 1
        previous_packet_number = packet_number
        expected_packet_number += 1
    print(f"Total missed packets: {missed_packets}")
    # Prepare the audio data for writing to a .wav file
    # Since the audio data is now a list of tuples, we need to flatten it
    audio_data_sorted = np.array([item for sublist in [item[1] for item in packets] for item in sublist], dtype=np.int32)

    # Write to a .wav file
    with wave.open('output.wav', 'wb') as wav_file:
        wav_file.setnchannels(CHANNELS)
        wav_file.setsampwidth(4) # Assuming 16-bit samples
        wav_file.setframerate(SAMPLE_RATE)
        wav_file.writeframes(audio_data_sorted.tobytes())

    print("Audio data written to output.wav")

if __name__ == "__main__":
    cProfile.run('main()', 'my-script.profile')