#include "PCM1808_I2S_UDP.h"
uint32_t mutex_save;
mutex_t my_mutex; // Mutex for thread safety

CircularBufferQueue::CircularBufferQueue() : fillIndex(1), emptyIndex(0)
{
}

char *CircularBufferQueue::getNextBufferAndUpdate(bool isFiller)
{
    uint32_t &currentIndex = isFiller ? fillIndex : emptyIndex;
    uint32_t otherIndex, temp;
    if (rp2040.fifo.available() == 0)
    {
        otherIndex = isFiller ? emptyIndex : fillIndex; // Return nullptr if the other op is not done.
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
        return buffers[currentIndex];
    }
    else
    {
        return nullptr; // Return null if the buffer is full/empty
    }
}

BufferEmptyer::BufferEmptyer(CircularBufferQueue &q) : queue(q)
{
    memset(temp_buffer, 0xff, BUFFER_SIZE);
}

void BufferEmptyer::emptyBuffer()
{
    while (!mutex_try_enter(&my_mutex, &mutex_save))
    {
        // Mutex is locked, so wait here.
    }
    char *buffer = queue.getNextBufferAndUpdate(false);
    mutex_exit(&my_mutex);
    if (buffer != nullptr)
    {
        // static uint32_t packet_number = 0;
        delayMicroseconds(100);
        udpData.beginPacket(remoteIp, DATA_UDPPORT);
        memcpy(temp_buffer, buffer, BUFFER_SIZE); // If we don't do this, it hangs in the udpData.write below.
        udpData.write((const uint8_t *)&temp_buffer, BUFFER_SIZE);
        udpData.endPacket();
        // if (*(uint32_t *)temp_buffer +1 != packet_number)
        // {
        //     Serial.printf("packet_number %d != %d\n", *(uint32_t *)temp_buffer, packet_number);
        // }
        // packet_number++;
        Serial.printf("Sent %d\n", *(int32_t *)buffer);
    }
}

BufferFiller::BufferFiller(CircularBufferQueue &q) : queue(q) {}

void BufferFiller::fillBuffer()
{
    while (!mutex_try_enter(&my_mutex, &mutex_save))
    {
        // Mutex is locked, so wait here.
    }
    char *buffer = queue.getNextBufferAndUpdate(true);
    mutex_exit(&my_mutex);
    if (buffer != nullptr)
    {
        static int32_t r, l, packet_number = 0;
        uint32_t bufferIndex = 4;
        while (bufferIndex < BUFFER_SIZE - 4) // Leave space for the packet number
        {
            i2s.read32(&l, &r);
            l = l << 9;
            r = r << 9;
            l &= 0xFFFFFF;                       // Keep only the lower 24 bits
            r &= 0xFFFFFF;                       // Keep only the lower 24 bits
            memcpy(buffer + bufferIndex, &l, 3); // Copy only the lower 3 bytes
            bufferIndex += 3;
            memcpy(buffer + bufferIndex, &r, 3); // Copy only the lower 3 bytes
            bufferIndex += 3;
        }
        // memcpy(buffer, &packet_number, sizeof(int32_t));  // For 32 bit samples
        memcpy(buffer, &packet_number, 4); 
        packet_number++;
        Serial.printf("Filled %d\n", packet_number);
    }
}
