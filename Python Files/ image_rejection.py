import numpy as np
import math

def calculate_image_rejection(I, Q):
    # Calculate the image rejection ratio
    image_rejection = 20*np.log10(np.abs(I - 1j*Q)/np.abs(I + 1j*Q))
    return image_rejection

# Generate example u and v signals
# u is rotating clockwise, v is rotating counter-clockwise
I = 1
Q = 1.01j + 0.01

# Calculate the image rejection ratio
image_rejection_ratio = calculate_image_rejection(I, Q)

print("Image Rejection Ratio (dB):", image_rejection_ratio)