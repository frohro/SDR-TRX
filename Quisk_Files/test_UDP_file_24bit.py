import socket
import struct
import wave
import numpy as np
import matplotlib.pyplot as plt
import time
import threading
import cProfile


# Constants
PORT = 12345
PACKET_SIZE = 1468 # 4 bytes for packet number + 183 * 8 bytes for int32_t pairs
SAMPLE_RATE =  48000 
CHANNELS = 2 # Stereo
NUM_PACKETS = 100
NUM_PLOT_POINTS = 100

lock = threading.Lock()

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
        with lock:
            start = time.time()
            packet_number = struct.unpack('<I', data[:4])[0] # Little endian

            # Unpack the audio data into 24-bit signed integers
            audio_data = np.frombuffer(data[4:], dtype=np.int8).reshape(-1, 3)
            # audio_data = (audio_data[:, 0] << 16) | (audio_data[:, 1] << 8) | audio_data[:, 2]
            # audio_data = (audio_data[:, 2] * 2**16 + audio_data[:, 1] * 2**8 + audio_data[:, 0])
            audio_data = ((audio_data[:, 2].astype(np.int32) << 16) | 
              (audio_data[:, 1].astype(np.int32) << 8) | 
              audio_data[:, 0].astype(np.int32))
                
            # Pair up the integers as left and right audio samples
            audio_data_pairs = list(zip(audio_data[::2], audio_data[1::2]))
            # audio_data_pairs = list(zip(audio_data[1::2], audio_data[::2]))
            packets.append((packet_number, audio_data_pairs))
            time_per_statement = time.time() - start

    print(f"Time per statement: {time_per_statement} seconds")
    print("audio_data: ", audio_data)

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
    if missed_packets != 0:
        print(f"This is {NUM_PACKETS*244/SAMPLE_RATE/missed_packets:.2f} seconds of audio data per problem.")
    
    # Prepare the audio data for writing to a .wav file
    # Since the audio data is now a list of tuples, we need to flatten it
    audio_data_sorted = np.array([sample for packet in packets for pair in packet[1] for sample in pair], dtype=np.int32)

    # Separate the left and right channels
    left_channel = audio_data_sorted[::2][:NUM_PLOT_POINTS]
    right_channel = audio_data_sorted[1::2][:NUM_PLOT_POINTS]
    print("Max of (Left + Right) is: ", max((0.5*left_channel + 0.5*right_channel)))
    print('Max of (Left - Right) is: ', max(0.5*left_channel - 0.5*right_channel))

    time_array = np.arange(len(left_channel))/SAMPLE_RATE
    plt.figure(figsize=(10, 6))
    plt.plot(time_array, left_channel, label='Left Channel')
    plt.plot(time_array, right_channel, label='Right Channel')
    # plt.plot(time_array, 0.5*left_channel + 0.5*right_channel, label='Average')
    # plt.plot(time_array, left_channel - right_channel, label='(Left - Right)*100')
    plt.legend()
    plt.xlabel('Time (s)')
    plt.title('Audio Data Received from Pico W')
    plt.show()


#    # Convert 24-bit audio to 16-bit
#     audio_data_16bit = (audio_data_sorted / np.power(2, 8)).astype(np.int16)

#     # Write to a .wav file (16-bith, so we don't have to use soundfile)
#     with wave.open('output.wav', 'wb') as wav_file:
#         wav_file.setnchannels(CHANNELS)
#         wav_file.setsampwidth(2)  # Set sample width to 2 bytes (16 bits)
#         wav_file.setframerate(SAMPLE_RATE)
#         wav_file.writeframes(audio_data_16bit.tobytes())

#     print("Audio data written to output.wav")

if __name__ == "__main__":
    cProfile.run('main()', 'my-script.profile')