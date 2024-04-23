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

# Create a figure
plt.figure(figsize=(10, 8))

# Plot the sum audio data
plt.subplot(2, 1, 1)
plt.plot(sum_audio)
plt.title('Left + Right')

# Plot the difference audio data
plt.subplot(2, 1, 2)
plt.plot(diff_audio)
plt.title('Left - Right')

# Show the plot
plt.tight_layout()
plt.show()