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

# Plot the audio data in chunks of 1000 samples
for i in range(0, len(audio_data), 1000):
    plt.figure(figsize=(10, 4))
    plt.plot(audio_data[i:i+1000])
    plt.show()