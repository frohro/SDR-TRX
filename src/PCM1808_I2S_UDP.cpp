#include "PCM1808_I2S_UDP.h"
uint32_t mutex_save;
mutex_t my_mutex; // Mutex for thread safety
// Initialize the static members
char BufferEmptyer::debugBuffer[1024] = {0};
int BufferEmptyer::debugBufferIndex = 0;

CircularBufferQueue::CircularBufferQueue() : fillIndex(1), emptyIndex(0)
{
}

char *CircularBufferQueue::getNextBufferAndUpdate(bool isFiller)
{
    uint32_t &currentIndex = isFiller ? fillIndex : emptyIndex;
    uint32_t otherIndex, temp;
    while (!mutex_try_enter(&my_mutex, &mutex_save))
    {
        // Mutex is locked, so wait here.
        Serial.printf("Mutex lock, isFiller: %d, t = %d\n", isFiller, millis());
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
            // BufferEmptyer::addDebugMessage("Filler buffer full, currentIndex: %d, time: %d\n",currentIndex, millis());
            Serial.printf("Filler buffer full, currentIndex: %d, time: %d\n", currentIndex, millis());
        }
        else
        {
            // BufferEmptyer::addDebugMessage("Emptyer buffer empty, currentIndex: %d, time: %d\n",currentIndex, millis());
        }
        mutex_exit(&my_mutex);
        return nullptr; // Return null if the buffer is full/empty
    }
}

BufferFiller::BufferFiller(CircularBufferQueue &q) : queue(q) {}

void BufferFiller::fillBuffer()
{
    // while (!mutex_try_enter(&my_mutex, &mutex_save))
    // {
    //     // Mutex is locked, so wait here.
    // }
    char *buffer = queue.getNextBufferAndUpdate(true);
    // mutex_exit(&my_mutex);
    if (buffer != nullptr)
    {
        static int32_t r, l, packet_number = 0;
        uint32_t bufferIndex = 4;
        while (bufferIndex < BUFFER_SIZE - 4) // Leave space for the packet number
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
        // Serial.printf("Filled %d\n", packet_number);
        packet_number++;
    }
}

BufferEmptyer::BufferEmptyer(CircularBufferQueue &q) : queue(q)
{
    if (RATE == 48000)
    {
        DELAY_TIME = 7500;
    }
    else if (RATE == 96000) // These delays changed the problem from BufferEmptyer to BufferFiller
    {
        DELAY_TIME = 00;
    }
    else
    {
        DELAY_TIME = 22500; // Hasn't been tested, but I think it should work for 16 ks/s.
    }
    memset(temp_buffer, 0xff, BUFFER_SIZE);
}

void BufferEmptyer::addDebugMessage(const char *format, ...)
{
    if (debugBufferIndex >= sizeof(debugBuffer))
    {
        // Buffer is full, cannot add more messages
        return;
    }

    va_list args;
    va_start(args, format);
    debugBufferIndex += vsnprintf(debugBuffer + debugBufferIndex, sizeof(debugBuffer) - debugBufferIndex, format, args);
    va_end(args);
}

void BufferEmptyer::printDebugMessages()
{
    Serial.printf("%s", debugBuffer);
    debugBufferIndex = 0; // Reset the buffer index
}

void BufferEmptyer::emptyBuffer()
{
    static uint32_t number_of_packets_sent = 0;
    // while (!mutex_try_enter(&my_mutex, &mutex_save))
    // {
    //     // Mutex is locked, so wait here.
    //     Serial.printf("Mutex is locked, so wait here at %d\n", millis());
    // }
    char *buffer = queue.getNextBufferAndUpdate(false);
    // mutex_exit(&my_mutex);
    if (buffer != nullptr)
    {
        delayMicroseconds(DELAY_TIME); // Tune this for minimmum number of missed packets.
        while (!udpData.beginPacket(remoteIp, DATA_UDPPORT))
        {
            delayMicroseconds(10);
            Serial.println("udpDat.beginPacket failed");
        }
        mutex_try_enter(&my_mutex, &mutex_save);
        memcpy(temp_buffer, buffer, BUFFER_SIZE); // If we don't do this, it hangs in the udpData.write below.
        mutex_exit(&my_mutex);
        uint32_t packet_number_in_packet = *(uint32_t *)temp_buffer;
        while (!udpData.write((const uint8_t *)&temp_buffer, BUFFER_SIZE))
        {
            delayMicroseconds(10);
            Serial.println("udpData.write failed");
        }
        while (!udpData.endPacket())
        {
            delayMicroseconds(10);
            Serial.println("udpData.endPacket failed");
        }
        number_of_packets_sent++;
        BufferEmptyer::addDebugMessage("Number of packets sent %d at %d.  Packet_number sent %d\n",
                                       number_of_packets_sent, millis(), packet_number_in_packet);
        if (number_of_packets_sent == 10)
        {
            BufferEmptyer::printDebugMessages();
        }
    }
}
