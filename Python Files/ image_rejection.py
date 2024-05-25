import numpy as np

def calculate_image_rejection(u, v):
    # Calculate the image rejection ratio
    image_rejection = 20*np.log10(np.real(u + np.conj(v))/np.imag(u + np.conj(v)))
    return image_rejection

# Generate example u and v signals
# u is rotating clockwise, v is rotating counter-clockwise
u = 1 + 1j
v = u*1.0*np.exp(1j*np.pi*0.01*180./np.pi)

# Calculate the image rejection ratio
image_rejection_ratio = calculate_image_rejection(u, v)

print("Image Rejection Ratio (dB):", image_rejection_ratio)