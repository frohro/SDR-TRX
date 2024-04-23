import wave
import numpy as np
import matplotlib.pyplot as plt

import numpy as np

# Open the .wav file
wav_file = wave.open('output.wav', 'rb')

# Read the audio data
nframes = wav_file.getnframes()
audio_data_bytes = np.frombuffer(wav_file.readframes(nframes), dtype=np.uint8)

# Close the .wav file
wav_file.close()

# Combine the bytes into 24-bit integers
audio_data = [audio_data_bytes[i] + (audio_data_bytes[i+1] << 8) + (audio_data_bytes[i+2] << 16) for i in range(0, len(audio_data_bytes), 3)]

# Separate the audio data into left and right channels
# Convert the lists to numpy arrays
left_channel = np.array(left_channel)
right_channel = np.array(right_channel)

# Calculate the sum and difference of the left and right channels
sum_audio = left_channel/10. + right_channel/10.
diff_audio = left_channel - right_channel

# Create a figure
plt.figure(figsize=(10, 12))

# Plot the sum audio data for the first 1000 points
plt.subplot(3, 1, 1)
plt.plot(sum_audio[:1000])
plt.title('(Left + Right)/10')

# Plot the difference audio data for the first 1000 points
plt.subplot(3, 1, 2)
plt.plot(diff_audio[:1000])
plt.title('Left - Right')

# Plot the left and right channels for the first 1000 points
plt.subplot(3, 1, 3)
plt.plot(left_channel[:1000], label='Left Channel')
plt.plot(right_channel[:1000], label='Right Channel')
plt.title('Left and Right Channels')
plt.legend()

# Show the plot
plt.tight_layout()
plt.show()