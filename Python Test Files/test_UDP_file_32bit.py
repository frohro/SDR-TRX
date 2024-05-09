import socket
import struct
import wave
import numpy as np
import cProfile
import matplotlib.pyplot as plt
import time

# Constants
PORT = 12345
PACKET_SIZE = 1444 # 4 bytes for packet number + 180 * 8 bytes for int32_t pairs
SAMPLE_RATE = 48000 # 96 ks/s
CHANNELS = 2 # Stereo
NUM_PACKETS = 1000
NUM_PLOT_POINTS = 1000

def main():
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024*1024)
    sock.bind(('', PORT))

    # Prepare to receive packets
    packets = []

    print("Listening for packets on port 12345...")
    while len(packets) < NUM_PACKETS:
        data, addr = sock.recvfrom(PACKET_SIZE)
        # start = time.time()
        packet_number = struct.unpack('<I', data[:4])[0] # Little endian
        # print("packet_number: ", packet_number)

        # Unpack the stereo audio data into a list of tuples (right, left)

        audio_data = struct.unpack('<360i', data[4:]) # Little endian

        # Pair up the integers as left and right audio samples
        audio_data_pairs = list(zip(audio_data[1::2], audio_data[::2]))
        packets.append((packet_number, audio_data_pairs))
        # time_per_statement = time.time() - start
        
    # print(f"Time per statement: {time_per_statement} seconds")
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
    unique_packets = NUM_PACKETS - missed_packets
    print(f"This is {unique_packets/float(NUM_PACKETS)*100:.8f}% of the total packets received.")
    # if missed_packets != 0:
    #     print(f"This is {NUM_PACKETS*244/SAMPLE_RATE/missed_packets:.2f} seconds of audio data per problem.")

    # # Prepare the audio data for writing to a .wav file
    # Since the audio data is now a list of tuples, we need to flatten it
    audio_data_sorted = np.array([item for sublist in [item[1] for item in packets] for item in sublist], dtype=np.int32)

    # Separate the left and right channels
    left_channel = np.array([item[0] for sublist in [item[1] for item in packets] for item in sublist], dtype=np.int32)[:NUM_PLOT_POINTS]
    right_channel = np.array([item[1] for sublist in [item[1] for item in packets] for item in sublist], dtype=np.int32)[:NUM_PLOT_POINTS]
    time_array = np.arange(len(left_channel))/SAMPLE_RATE
    plt.figure(figsize=(10, 6))
    plt.plot(time_array, left_channel, label='Left Channel')
    plt.plot(time_array, right_channel, label='Right Channel')
    plt.legend()
    plt.xlabel('Time (s)')
    plt.title('Audio Data Received from Pico W')
    plt.show()

    # # Write to a .wav file
    # with wave.open('output.wav', 'wb') as wav_file:
    #     wav_file.setnchannels(CHANNELS)
    #     wav_file.setsampwidth(4) # Assuming 16-bit samples
    #     wav_file.setframerate(SAMPLE_RATE)
    #     wav_file.writeframes(audio_data_sorted.tobytes())

    # print("Audio data written to output.wav")

if __name__ == "__main__":
    cProfile.run('main()', 'my-script.profile')