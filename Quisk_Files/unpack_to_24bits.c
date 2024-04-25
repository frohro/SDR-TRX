#include <stdint.h>

void convert_audio_data(uint8_t* data, int32_t* output, int len) {
    for (int i = 4; i < len; i += 3) {
        output[i/3] = data[i] | (data[i+1] << 8) | ((int8_t)data[i+2] << 16);
    }
}