import numpy as np
import math

def calculate_image_rejection(I, Q):
    # Calculate the image rejection ratio
    image_rejection = 20*np.log10(np.abs(I - 1j*Q)/np.abs(I + 1j*Q))
    return image_rejection

# Generate example u and v signals
# u is rotating clockwise, v is rotating counter-clockwise
I = 1
Q = 1.01j + 0.01  # A 1% error in real and imaginary parts

# Calculate the image rejection ratio
image_rejection_ratio = calculate_image_rejection(I, Q)

print("Image Rejection Ratio (dB):", image_rejection_ratio)
# When I run this I get 43 dB for a 1% error in the real and imaginary parts of the signal which seems about right.