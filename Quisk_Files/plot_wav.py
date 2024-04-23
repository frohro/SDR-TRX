import wave
import numpy as np
import matplotlib.pyplot as plt

# Open the .wav file
wav_file = wave.open('output.wav', 'rb')

# Read the audio data
nframes = wav_file.getnframes()
audio_data = np.frombuffer(wav_file.readframes(nframes), dtype=np.int32)

# Close the .wav file
wav_file.close()

# Separate the audio data into left and right channels
left_channel = audio_data[::2]
right_channel = audio_data[1::2]

# Calculate the sum and difference of the left and right channels
sum_audio = left_channel + right_channel
diff_audio = left_channel - right_channel

# Plot the sum and difference audio data in chunks of 1000 samples
for i in range(0, len(sum_audio), 1000):
    plt.figure(figsize=(10, 4))
    plt.plot(sum_audio[i:i+1000])
    plt.title('Left + Right')
    plt.show()

for i in range(0, len(diff_audio), 1000):
    plt.figure(figsize=(10, 4))
    plt.plot(diff_audio[i:i+1000])
    plt.title('Left - Right')
    plt.show()