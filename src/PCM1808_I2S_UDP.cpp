#include "PCM1808_I2S_UDP.h"

uint32_t mutex_save;
mutex_t my_mutex; // Mutex for thread safety

CircularBufferQueue::CircularBufferQueue() : fillIndex(1), emptyIndex(0)
{
    rp2040.wdt_begin(7000);
}

char *CircularBufferQueue::getNextBufferAndUpdate(bool isFiller)
{
    uint32_t &currentIndex = isFiller ? fillIndex : emptyIndex;
    uint32_t otherIndex, temp;
    while (!mutex_try_enter(&my_mutex, &mutex_save))
    {
        // Mutex is locked, so wait here.
        // Serial.printf("Mutex lock, isFiller: %d, t = %d\n", isFiller, millis());
    }
    if (rp2040.fifo.available() == 0) // other_index is still the same.
    {

        otherIndex = isFiller ? emptyIndex : fillIndex;
    }
    else
    {
        while (rp2040.fifo.pop_nb(&temp)) // Gets most recent otherIndex, empties FIFO
        {
            otherIndex = temp;
        }
    }
    if ((currentIndex + 1) % QUEUE_SIZE != otherIndex) // If we can move to the next buffer...
    {
        currentIndex = (currentIndex + 1) % QUEUE_SIZE;
        rp2040.fifo.push(currentIndex);
        mutex_exit(&my_mutex);
        return buffers[currentIndex];
    }
    else
    {
        if (isFiller)
        {
            Serial.printf("Filler buffer full, currentIndex: %lu, time: %lu\n", currentIndex, millis());
        }
        mutex_exit(&my_mutex);
        return nullptr; // Return null if the buffer is full/empty
    }
}

BufferFiller::BufferFiller(CircularBufferQueue &q) : queue(q) {}

void BufferFiller::fillBuffer()
{
    char *buffer = queue.getNextBufferAndUpdate(true);

    if (buffer != nullptr)
    {
        static int32_t packet_number = 0; //, skip_counter = SAMPLES_PER_SKIP_A_SAMPlE;
        uint32_t bufferIndex = 4;
        int32_t r, l;
        while (bufferIndex < BUFFER_SIZE) // Leave space for the packet number
        {
            i2s.read32(&l, &r);
            if (BITS_PER_SAMPLE_SENT == 24)
            {
                memcpy(buffer + bufferIndex, &l, 3); // Copy only the lower 3 bytes
                bufferIndex += 3;
                memcpy(buffer + bufferIndex, &r, 3); // Copy only the lower 3 bytes
                bufferIndex += 3;
            }
            else if (BITS_PER_SAMPLE_SENT == 32)
            {
                l = l << 8;                                        // Keep only the lower 24 bits
                r = r << 8;                                        // Keep only the lower 24 bits
                memcpy(buffer + bufferIndex, &l, sizeof(int32_t)); // Copy only the lower 3 bytes
                bufferIndex += sizeof(int32_t);
                memcpy(buffer + bufferIndex, &r, sizeof(int32_t)); // Copy only the lower 3 bytes
                bufferIndex += sizeof(int32_t);
            }
        }
        memcpy(buffer, &packet_number, sizeof(int32_t));
        // Serial.printf("Filled %ld, t %ld\n", packet_number, micros());
        packet_number++;
    }
}

BufferEmptyer::BufferEmptyer(CircularBufferQueue &q) : queue(q)
{
    memset(temp_buffer, 0xff, BUFFER_SIZE);
}

void BufferEmptyer::emptyBuffer()
{
    char *buffer = queue.getNextBufferAndUpdate(false);
        // Feed the watchdog to reset the timer
    rp2040.wdt_reset();
    if (buffer != nullptr)
    {
        delayMicroseconds(1200); // For 48 ks/s, 32-bit and 1200 no packet loss.
        // delayMicroseconds(DELAY_TIME); // Tune this for minimmum number of missed packets.
        while (!udpData.beginPacket(remoteIp, DATA_UDPPORT))
        {
            delayMicroseconds(1100);
            Serial.println("udpDat.beginPacket failed");
        }
        memcpy(temp_buffer, buffer, BUFFER_SIZE); // If we don't do this, it hangs in the udpData.write below.
        while (!udpData.write((const uint8_t *)&temp_buffer, BUFFER_SIZE))
        {
            Serial.println("udpData.write failed");
        }
        while (!udpData.endPacket())
        {

            Serial.println("udpData.endPacket failed");
        }
        // Serial.printf("Emptied %d, t %d\n", *(uint32_t *)buffer,micros());
    }
}
