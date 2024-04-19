#include "PCM1808_I2S_UDP.h"
uint32_t mutex_save;
mutex_t my_mutex; // Mutex for thread safety
WiFiUDP udpData;  
I2S i2s(INPUT);

CircularBufferQueue::CircularBufferQueue() : fillIndex(1), emptyIndex(0)
{
}

char* CircularBufferQueue::getNextBufferAndUpdate(bool isFiller)
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
        udpData.beginPacket(remoteIP, DATA_UDPPORT); 
        memcpy(temp_buffer, buffer, BUFFER_SIZE);     // If we don't do this, it hangs in the udpData.write below.
        udpData.write((const uint8_t *)&temp_buffer, BUFFER_SIZE);
        udpData.endPacket();
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
            memcpy(buffer + bufferIndex, &l, sizeof(int32_t)); // We could speed this up for the network by sending 24 bits instead of 32.
            bufferIndex += sizeof(int32_t);
            memcpy(buffer + bufferIndex, &r, sizeof(int32_t));
            bufferIndex += sizeof(int32_t);
        }
        memcpy(buffer, &packet_number, sizeof(int32_t));
        packet_number++;
        Serial.printf("Filled %d\n", packet_number);
    }
}