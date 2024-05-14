/*
 * This Pico W program will connect to the PCM1808 via I2S and read the data it rate samples per second.
 * It then sends the data over WIFI using UDP.  To receive it and plot the results, you can use the
 * python scripts and this sequence in Linux:
 * $ rm sound_data.csv
 * $ nc -u -l 12345 > sound_data.csv
 * ^C
 * $ python3 plot_sound_data.py
 * The ^C is to stop the nc process which is receiving the data and saving it to sound_data.csv.
 * You can see that every once in a while there will be what appears to be a zero in the data.
 * We will have to remove them.  We also have to remove about 90 samples every second at 96 kHz
 * sampling rate.  (Less at slower sampling rates, because the Pico does not have exactly the right
 * crystal frequency to get exactly the audio rate we need.  We could reduce the load on the network
 * by sending the data in a binary way.  It appears we are getting almost 3.2 Mbits/s through the
 * WIFI.  This is what the Monitor says, but I calculate 20 Mbit/s.  Why? I think our tests at 96 kHz
 * were flawed, and not correct, as I connot reproduce them.
 *
 * Author: Rob Frohne, KL7NA, with help from Perplexity and Github Copilot.
 * 3/9/2024
 */
#include <Arduino.h>
#include <Wire.h>
#include <I2S.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <si5351.h>
#include <JTEncode.h>
#include <pico/mutex.h>
#include "PCM1808_I2S_UDP.h"

// To do:
// 1) We need to add I2C code eand calibrate thu INA219.
// 2) Refactor this into a library so you can do a receiver, transmitter
// or transceiver.  Use OOP.  Make the code maintainable..

// These are things you might want to change for your situation.
const int RATE = 48000;              // Your network needs to handle this, but 96000 should also work, but misses some packets.
const int BITS_PER_SAMPLE_SENT = 32; // 24 or 32, 32 is less packet loss for some strange reason.
const int MCLK_MULT = 256;           //
// const char *STASSID = "rosbots";
// const char *PASSWORD = "ros2bots"; // In case you have a password,
// you must add this to the WiFi.begin() call, or if you have no
// password, you need to take the PASSWORD arguement out.
const unsigned int DATA_UDPPORT = 12345;
const unsigned int COMMAND_UDPPORT = 12346;
const unsigned int BROADCAST_UDP_PORT = 12347;
const char *VERSION_NUMBER = "0.1.1";
IPAddress quiskIP(192, 168, 1, 101); // This is the IP address of the computer running Quisk.
// It will be replaced with the IP of the computer that sends the command.
// Radio specific parameters go here.
const int FREQ_LIMIT_LOWER = 3500000;
const int FREQ_LIMIT_UPPER = 30000000;
const int RX_SWITCH = 10;          // GPIO pin for RX/TX switch (High is RX)
const int SMPS_ENABLE = 11;        // GPIO pin for SMPS enable
const int CAL_FACTOR = 1.00007587; // Calibration factor for the Si5351 (multiply by this number to get the right frequency)
uint_fast32_t rx_freq = 14074000;  // 20 meter FT8 frequency
uint_fast32_t tx_freq = 14074000;  // 20 meter FT8 frequency
int rx_relay_state = 0;            // 0 is RX, 1 is TX
int tx_state = 0;                  // 0 is off, 1 is on
bool data_sending = false;         // Are we sending data over UDP?
bool useUDP = false;               // If false, use UART instead of UDP.
const int SERIAL_TIMEOUT = 20000;  // 20 seconds
WiFiUDP udpData;                   // UDP object for data packets  (Is this a server or a client?)
WiFiUDP udpCommand;                // UDP object for control packets

Si5351 si5351(0x60); // 0x60 is the normal I2C address for the Si5351A or MS5351M

CircularBufferQueue bufferQueue;

// Mode defines
const int JT9_TONE_SPACING = 174;  // ~1.74 Hz
const int JT65_TONE_SPACING = 269; // ~2.69 Hz
const int JT4_TONE_SPACING = 437;  // ~4.37 Hz
const int WSPR_TONE_SPACING = 146; // ~1.46 Hz
const int FSQ_TONE_SPACING = 879;  // ~8.79 Hz
const int FT8_TONE_SPACING = 625;  // ~6.25 Hz
const int FT4_TONE_SPACING = 2083; // ~20.83 Hz

const int JT9_DELAY = 576;     // Delay value for JT9-1
const int JT65_DELAY = 371;    // Delay in ms for JT65A
const int JT4_DELAY = 229;     // Delay value for JT4A
const int WSPR_DELAY = 682;    // Delay value for WSPR
const int FSQ_2_DELAY = 500;   // Delay value for 2 baud FSQ
const int FSQ_3_DELAY = 333;   // Delay value for 3 baud FSQ
const int FSQ_4_5_DELAY = 222; // Delay value for 4.5 baud FSQ
const int FSQ_6_DELAY = 167;   // Delay value for 6 baud FSQ
const int FT8_DELAY = 157;     // Delay value for FT8
const int FT4_DELAY = 45;      // Need to substract 3ms due to PLL delay

const int FT4_SYMBOL_COUNT = 103; // 105?   https://wsjt.sourceforge.io/FT4_FT8_QEX.pdf

const uint_fast32_t JT9_DEFAULT_FREQ = 14078700UL;
const uint_fast32_t JT65_DEFAULT_FREQ = 14078300UL;
const uint_fast32_t JT4_DEFAULT_FREQ = 14078500UL;
const uint_fast32_t WSPR_DEFAULT_FREQ = 14038600UL;
const uint_fast32_t FSQ_DEFAULT_FREQ = 14005350UL; // Base freq is 1350 Hz higher than dial freq in USB
const uint_fast32_t FT8_DEFAULT_FREQ = 14074000UL;
const uint_fast32_t FT4_DEFAULT_FREQ = 14047500UL;

// Enumerations
enum mode
{
  MODE_JT9,
  MODE_JT65,
  MODE_JT4,
  MODE_WSPR,
  MODE_FSQ_2,
  MODE_FSQ_3,
  MODE_FSQ_4_5,
  MODE_FSQ_6,
  MODE_FT8,
  MODE_FT4
};
mode DEFAULT_MODE = MODE_FT8;

uint16_t offset = 1200; // Baseband offset in Hz
uint8_t tx_buffer[255];
enum mode cur_mode = DEFAULT_MODE;
uint8_t symbol_count;
uint16_t tone_delay, tone_spacing;
bool message_available = false;
unsigned int timeout;

// To have the Si5351 produce the I/Q signals below 4.8 MHz, you need to modify the si5351.h
// header so SI5351_PLL_VCO_MIN is 380000000 (380 MHz).
static void set_rx_freq(uint_fast32_t freq)
{
  // Use the Si5351 to produce the I/Q on CLK0 and CLK1.
  rx_freq = freq; // Assuming this does not fail.  Probably should add some error checking.
  uint64_t pll_freq;
  uint_fast8_t mult;
  // mult must be less than 128 (7 bits) according to documentation
  if (freq >= 26470588)
  {
    mult = 22;
  }
  else if (freq >= 18000000)
  {
    mult = 34;
  }
  else if (freq > 12000000)
  {
    mult = 50;
  }
  else if (freq >= 8000000)
  {
    mult = 75;
  }
  else if (freq > 5357143)
  {
    mult = 112;
  }
  else
  {
    mult = 126;
  }
  pll_freq = (uint64_t)mult * freq;
  // Serial.printf("Setting RX frequency to %d Hz\n", freq);
  // Serial.printf("PLL frequency is %llu Hz\n", pll_freq);
  si5351.set_freq_manual(freq * 100ULL * CAL_FACTOR, pll_freq * 100ULL, SI5351_CLK0);
  si5351.set_freq_manual(freq * 100ULL * CAL_FACTOR, pll_freq * 100ULL, SI5351_CLK1);
  // Now we can set CLK1 to have a 90 deg phase shift by entering
  // mult in the CLK1 phase register, since the ratio of the PLL to
  // the clock frequency is mult.
  si5351.set_phase(SI5351_CLK0, 0);
  si5351.set_phase(SI5351_CLK1, mult);
  // We need to reset the PLL before they will be in phase alignment
  si5351.pll_reset(SI5351_PLLA);
}

void tx()
{
  if (useUDP)
  {
    data_sending = false;
  }
  digitalWrite(RX_SWITCH, LOW);    // Set TX mode on T/R switch
  digitalWrite(LED_BUILTIN, HIGH); // Turn on the LED
  digitalWrite(SMPS_ENABLE, HIGH); // Turn on the SMPS for higher power
  delay(100);
  rx_relay_state = 0;
  si5351.output_enable(SI5351_CLK2, 1);
  tx_state = 1;
}

void rx()
{ // Do we really need two functions for rx and tx? maybe a parameter?
  if (useUDP)
  {
    data_sending = true;
  }
  si5351.output_enable(SI5351_CLK2, 0);
  tx_state = 0;
  digitalWrite(RX_SWITCH, HIGH);
  digitalWrite(LED_BUILTIN, LOW); // Turn off the LED
  rx_relay_state = 1;
  digitalWrite(SMPS_ENABLE, LOW); // Turn off the SMPS for lower noise (perhaps)
}

void set_tx_freq(uint32_t freq)
{
  // Let driver choose PLL settings. May glitch when changing frequencies.
  tx_freq = freq;
  // Serial.print("Setting TX frequency to ");
  // Serial.println(tx_freq);
  si5351.set_freq(tx_freq * 100ULL * CAL_FACTOR, SI5351_CLK2);
}

// Loop through the string, transmitting one character at a time.
void transmit()
{
  uint8_t i;
  unsigned long digital_tx_freq;
  digital_tx_freq = tx_freq + offset;
  // Reset the tone to the base frequency and turn on the output
  tx();
  // Now transmit the channel symbols
  if (cur_mode == MODE_FSQ_2 || cur_mode == MODE_FSQ_3 || cur_mode == MODE_FSQ_4_5 || cur_mode == MODE_FSQ_6)
  {
    uint8_t j = 0;
    while (tx_buffer[j++] != 0xff)
      ;
    symbol_count = j - 1;
  }
  for (i = 0; i < symbol_count; i++)
  {
    si5351.set_freq((digital_tx_freq) * 100ULL + (tx_buffer[i] * tone_spacing), SI5351_CLK2);
    delay(tone_delay);
  }
  // Back to receive
  Serial.println("Transmitting complete.");
  delay(100);
  rx();
}

void setup_mode(enum mode sel_mode)
{
  // Set the proper frequency, tone spacing, symbol count, and
  // tone delay depending on mode
  switch (sel_mode)
  {
  case MODE_JT9:
    tx_freq = JT9_DEFAULT_FREQ;
    symbol_count = JT9_SYMBOL_COUNT; // From the library defines
    tone_spacing = JT9_TONE_SPACING;
    tone_delay = JT9_DELAY;
    break;
  case MODE_JT65:
    tx_freq = JT65_DEFAULT_FREQ;
    symbol_count = JT65_SYMBOL_COUNT; // From the library defines
    tone_spacing = JT65_TONE_SPACING;
    tone_delay = JT65_DELAY;
    break;
  case MODE_JT4:
    tx_freq = JT4_DEFAULT_FREQ;
    symbol_count = JT4_SYMBOL_COUNT;
    tone_spacing = JT4_TONE_SPACING;
    tone_delay = JT4_DELAY;
    break;
  case MODE_WSPR:
    tx_freq = WSPR_DEFAULT_FREQ;
    symbol_count = WSPR_SYMBOL_COUNT; // From the library defines
    tone_spacing = WSPR_TONE_SPACING;
    tone_delay = WSPR_DELAY;
    break;
  case MODE_FT8:
    tx_freq = FT8_DEFAULT_FREQ;      // At this point, we are letting Quisk & WJST-X set the frequency. Maybe delete these.
    symbol_count = FT8_SYMBOL_COUNT; // From the library defines
    tone_spacing = FT8_TONE_SPACING;
    tone_delay = FT8_DELAY;
    break;
  case MODE_FT4:
    tx_freq = FT4_DEFAULT_FREQ;
    symbol_count = FT4_SYMBOL_COUNT;
    tone_spacing = FT4_TONE_SPACING;
    tone_delay = FT4_DELAY;
    break;
  case MODE_FSQ_2:
    tx_freq = FSQ_DEFAULT_FREQ;
    tone_spacing = FSQ_TONE_SPACING;
    tone_delay = FSQ_2_DELAY;
    break;
  case MODE_FSQ_3:
    tx_freq = FSQ_DEFAULT_FREQ;
    tone_spacing = FSQ_TONE_SPACING;
    tone_delay = FSQ_3_DELAY;
    break;
  case MODE_FSQ_4_5:
    tx_freq = FSQ_DEFAULT_FREQ;
    tone_spacing = FSQ_TONE_SPACING;
    tone_delay = FSQ_4_5_DELAY;
    break;
  case MODE_FSQ_6:
    tx_freq = FSQ_DEFAULT_FREQ;
    tone_spacing = FSQ_TONE_SPACING;
    tone_delay = FSQ_6_DELAY;
    break;
  }
}

uint8_t si5351_init()
{
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  // Set output frequencies
  set_rx_freq(rx_freq);
  set_tx_freq(tx_freq);
  // Enable RX, and disable TX by default.
  si5351.output_enable(SI5351_CLK0, 1);
  si5351.output_enable(SI5351_CLK1, 1);
  si5351.output_enable(SI5351_CLK2, 0);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA); // Set for max power on TX
  // Read the status register and return the chip revision ID.
  si5351.update_status();
  return si5351.dev_status.REVID;
}

I2S i2s(INPUT);

void processCommandUDP()
{
  bool no_error;
  char received = 0;
  String command;

  // Check if there's any serial data available.
  if (Serial.available())
  {
    command = Serial.readStringUntil('\n');
    no_error = true;
    // Process the received command
    if (command.startsWith("VER"))
    {
      useUDP = false; // Set useUDP to false if the UART gives us a VER command.
      Serial.printf("VER, %s\r\n", VERSION_NUMBER);
    }
    return;
  }
  int command_packetSize = udpCommand.parsePacket();
  if (command_packetSize)
  {
    char command_packetBuffer[UDP_TX_PACKET_MAX_SIZE];
    int len = udpCommand.read(command_packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    if (len > 0)
    {
      command_packetBuffer[len] = 0; // Null terminate the string.
      received = command_packetBuffer[0];
    }
    command = String(command_packetBuffer);
    no_error = true;
    // UART Prints for debugging.  Eventually take all the Serial.prints out of this function.
    Serial.print("Received packet of size ");
    Serial.println(len);
    Serial.print("From ");
    Serial.print(udpCommand.remoteIP());
    Serial.print(", port ");
    Serial.println(udpCommand.remotePort());
    Serial.print("Message: ");
    Serial.println(command);
    // Process the received command
    quiskIP = udpCommand.remoteIP();
    Serial.print("received is ");
    Serial.println(received);
    if ((received == 'm') || // This whole if statement is a way of combining my code with
        (received == 'o') || // wsjt-transceiver.ino.  I'm not sure if it is the best way,
        (received == 'e') || // but it seperates the wsjt-x commands from the original commands..
        (received == 'w') ||
        (received == 't') ||
        (received == 'p') ||
        (received == 'r') ||
        (received == 'f') ||
        (received < 32)) // Check for a message which is a control character in ASCII.
    {
      // New message
      if (received == 'm')
      {
        Serial.println("Received an m.");
        int packetSize = udpCommand.parsePacket();
        if (packetSize)
        {
          // If there's a packet available, read it into a buffer
          int len = udpCommand.read(tx_buffer, 255);
          if (len == symbol_count)
          {
            tx_buffer[len] = 0;
            Serial.println((char *)tx_buffer);
          }
          else
          {
            Serial.println("Packet was not symbol_count bytes long.");
          }
        }
        message_available = true;
        udpCommand.write("m");
      }
      // Change offset
      else if (received == 'o')
      {
        char incomingPacket[255];
        // Serial.println("Received an o.");
        // Offset encoded in two bytes
        int packetSize = udpCommand.parsePacket();
        if (packetSize)
        {
          // If there's a packet available, read it into a buffer
          int len = udpCommand.read(incomingPacket, 255);
          if (len == 2)
          {
            incomingPacket[len] = 0;
          }
          else
          {
            Serial.println("Offset packet was not 2 bytes long.");
          }
        }
        offset = (incomingPacket[0]) + (incomingPacket[1] << 8);
        udpCommand.write("o");
        // Serial.println("Wrote o.");
      }

      // Switch mode = FT8  Note: We start in FT8 mode and don't sync until a change mode is sent.
      else if (received == 'e')
      {
        cur_mode = MODE_FT8;
        setup_mode(cur_mode);
        udpCommand.write("e");
        message_available = false;
      }
      // Switch mode = FT4
      else if (received == 'f')
      {
        cur_mode = MODE_FT4;
        setup_mode(cur_mode);
        udpCommand.write("f");
        message_available = false;
      }
      // WSPR Mode
      else if (received == 'w')
      {
        cur_mode = MODE_WSPR;
        setup_mode(cur_mode);
        message_available = false;
      }
      // Transmit
      else if (received == 't')
      {
        if (message_available)
          transmit();
      }
      // Pre transmit
      else if (received == 'p')
      {
        // tx_enable();  Don't need this.
      }
      else if (received == 'r')
      {
        // Send indication for ready!
        udpCommand.write("r");
      }
    }
    else
    {
      if (command.startsWith("VER")) // Whatever way we get VER, sets useUDP.
      {
        udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
        Serial.println("VER received!");
        Serial.println("Began Packet.");
        Serial.printf("VER,%s\r\n", VERSION_NUMBER);
        udpCommand.printf("VER,%s\r\n", VERSION_NUMBER);
        udpCommand.endPacket();
        useUDP = true; // Set useUDP to true
        // Determine whether to use UDP or UART.
      }
      else if (command.startsWith("START_I2S_UDP"))
      {
        Serial.println("START_I2S_UDP received!");
        data_sending = true;
        udpData.begin(DATA_UDPPORT); // Initialize UDP for data port
        i2s.begin();
      }
      else if (command.startsWith("STOP"))
      {
        Serial.println("STOP received!");
        data_sending = false;
        udpData.stop();
        i2s.end();
      }
      else if (command.startsWith("FREQ"))
      {
        Serial.println("FREQ received!");
        int commaIndex = command.indexOf(',');
        if (commaIndex != -1)
        {
          String freqStr = command.substring(commaIndex + 1);
          uint32_t freq = freqStr.toInt();
          set_rx_freq(freq);
          set_tx_freq(freq);
          udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
          udpCommand.printf("FREQ,%ld\r\n", freq);
          udpCommand.endPacket();
        }
        else
        {
          udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
          udpCommand.printf("FREQ,%u\r\n", rx_freq);
          udpCommand.endPacket();
        }
      }
      else if (command.startsWith("TX_FREQ"))
      {
        Serial.println("TX_FREQ received!");
        int commaIndex = command.indexOf(',');
        if (commaIndex != -1)
        {
          String freqStr = command.substring(commaIndex + 1);
          uint32_t freq = freqStr.toInt();
          set_tx_freq(freq);
          udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
          udpCommand.printf("TX_FREQ,%ld\r\n", freq);
          udpCommand.endPacket();
        }
        else
        {
          udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
          udpCommand.printf("TX_FREQ,%d\r\n", tx_freq);
          udpCommand.endPacket();
        }
      }
      else if (command.startsWith("USE_UART"))
      {
        useUDP = false; // Set useUDP to false
        data_sending = false;
        udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
        udpCommand.printf("USE_UART\r\n");
        udpCommand.endPacket();
      }
      else if (command.startsWith("TX"))
      {
        Serial.println("TX received!");
        tx();
        udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
        udpCommand.printf("TX\r\n");
        udpCommand.endPacket();
      }
      else if (command.startsWith("RX"))
      {
        Serial.println("RX received!");
        rx();
        udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
        udpCommand.printf("RX\r\n");
        udpCommand.endPacket();
      }
      else
      {

        Serial.println("Unknown command received!");
        udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
        udpCommand.write("ERROR\r\n");
        udpCommand.endPacket();
        no_error = false;
      }
    }
    if (no_error)
    {
      udpCommand.beginPacket(quiskIP, udpCommand.remotePort());
      udpCommand.write("OK\r\n"); // Do we need an Ok after an error?
      udpCommand.endPacket();
    }
  }
}

void processCommandUART()
{
  bool no_error;
  String command;
  char received = 0;
  unsigned char rec_byte[2];
  unsigned char msg_index;
  // Check if there's any serial data available
  if (Serial.available() > 0)
    received = Serial.read();
  // Serial.printf("CammandUART received is: ", received);
  if ((received == 'm') || // This whole if statement is a way of combining my code with
      (received == 'o') || // wsjt-transceiver.ino.  I'm not sure if it is the best way.
      (received == 'e') ||
      (received == 'w') ||
      (received == 't') ||
      (received == 'p') ||
      (received == 'r') ||
      (received == 'f') ||
      (received < 32)) // Check for a message which is a control character in ASCII.
  {
    // New message
    if (received == 'm')
    {
      msg_index = 0;
      // timeout = 0;
      while (msg_index < symbol_count) // && timeout < SERIAL_TIMEOUT)
      {
        if (Serial.available() > 0)
        {
          received = Serial.read();
          tx_buffer[msg_index] = received;
          msg_index++;
        }
      }
      message_available = true;
      Serial.print("m");
    }

    // Change offset
    else if (received == 'o')
    {
      msg_index = 0;
      // Offset encoded in two bytes
      while (msg_index < 2)
      {
        if (Serial.available() > 0)
        {
          rec_byte[msg_index] = Serial.read();
          msg_index++;
        }
      }
      offset = rec_byte[0] + (rec_byte[1] << 8);
      Serial.print("o");
    }

    // Switch mode = FT8  Note: We start in FT8 mode and don't sync until a change mode is sent.
    else if (received == 'e')
    {
      cur_mode = MODE_FT8;
      setup_mode(cur_mode);
      Serial.print("e");
      message_available = false;
    }

    // Switch mode = FT4
    else if (received == 'f')
    {
      cur_mode = MODE_FT4;
      setup_mode(cur_mode);
      Serial.print("f");
      message_available = false;
    }

    // WSPR Mode
    else if (received == 'w')
    {
      cur_mode = MODE_WSPR;
      setup_mode(cur_mode);
      message_available = false;
    }

    // Transmit
    else if (received == 't')
    {
      if (message_available)
        transmit();
    }

    // Pre transmit
    else if (received == 'p')
    {
      // tx_enable();  Don't need this.
    }
    else if (received == 'r')
    {
      // Send indication for ready!
      Serial.print("r");
    }
  }
  else
  {
    command = received + Serial.readStringUntil('\n');
    no_error = true;
    // Process the received command
    if (command.startsWith("VER"))
    {
      Serial.printf("VER,%s\r\n", VERSION_NUMBER);
      // useUDP = false; // Set useUDP to false
      //  The first thing Quisk does is to check VER.
      //  So if it uses UART, we will use UART.
    }
    // else if (command.startsWith("START_I2S_UDP"))
    // {
    //   Serial.println("START_I2S_UDP received!");
    //   data_sending = true;
    //   // Also need to connect to WiFi, setup ports for UDP Commands, etc  This needs fixing.
    //   i2s.begin();
    // }
    // else if (command.startsWith("STOP_I2S_UDP"))
    // {
    //   Serial.println("STOP_I2S_UDP received!");
    //   data_sending = false;
    //   i2s.end();
    // }
    else if (command.startsWith("FREQ"))
    {
      int commaIndex = command.indexOf(',');
      if (commaIndex != -1)
      {
        String freqStr = command.substring(commaIndex + 1);
        uint32_t freq = freqStr.toInt();
        set_rx_freq(freq);
        Serial.printf("FREQ,%ld\r\n", freq);
      }
      else
      {
        Serial.printf("FREQ,%u\r\n", rx_freq);
      }
    }
    else if (command.startsWith("TX_FREQ"))
    {
      int commaIndex = command.indexOf(',');
      if (commaIndex != -1)
      {
        String freqStr = command.substring(commaIndex + 1);
        uint32_t freq = freqStr.toInt();
        set_tx_freq(freq);
        Serial.printf("TX_FREQ,%ld\r\n", freq);
      }
      else
      {
        Serial.printf("TX_FREQ,%u\r\n", tx_freq);
      }
    }
    // else if (command.startsWith("USE_UDP"))
    // {
    //   useUDP = true; // Set useUDP to true
    //   // This needs fixed.  We need to connect to WIFI, etc. too.
    //   Serial.printf("USE_UDP\r\n");
    // }
    else if (command.startsWith("TX"))
    {
      tx();
      Serial.printf("TX\r\n");
    }
    else if (command.startsWith("RX"))
    {
      rx();
      Serial.printf("RX\r\n");
    }
    else
    {
      Serial.println("Unknown command received!");
      Serial.write("ERROR\r\n");
      no_error = false;
    }
    if (no_error)
    {
      Serial.write("OK\r\n"); // Do we need an Ok after an error?
    }
  }
}
void find_quisk_IP()
{
  // Set up UDP for receiving broadcast messages
  WiFiUDP broadcast_udp;
  broadcast_udp.begin(BROADCAST_UDP_PORT);

  // Wait for a broadcast message
  const char *message = "Quisk";
  int messageSize = strlen(message) + 1;

  while (true)
  {
    int packetSize = broadcast_udp.parsePacket();
    if (packetSize)
    {
      // We've received a packet, read the data from it
      char packetBuffer[messageSize];
      broadcast_udp.read(packetBuffer, messageSize - 1);
      packetBuffer[messageSize - 1] = '\0'; // Add a null terminator
      Serial.printf("Received broadcast message: %s\n", packetBuffer);

      // Check the identifier
      if (String(packetBuffer) == message)
      {
        // Get the sender's IP address
        quiskIP = broadcast_udp.remoteIP();
        Serial.printf("Quisk IP address: %s\n", quiskIP.toString().c_str());
        break;
      }
    }
    // Delay between checks
    rp2040.wdt_reset();
    delay(10);
  }
}

bool SetupWiFi(void)
{
  // Scan for available networks
  int n = WiFi.scanNetworks();
  String bestSSID;
  int32_t bestRSSI = -1000;

  for (int i = 0; i < n; i++)
  {
    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);

    // Check if this is one of the SSIDs we're interested in and has a stronger signal
    // if ((ssid == "Frohne-2.4GHz" || ssid == "Frohne-Shop-2.4GHz") && rssi > bestRSSI)
    if (rssi > bestRSSI)
    {
      bestSSID = ssid;
      bestRSSI = rssi;
    }
  }

  // Connect to the best SSID
  WiFi.begin(bestSSID.c_str());

  for (int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; i++)
  {
    delay(1000); // Delay for 1 second
    rp2040.wdt_reset();
    Serial.println("Trying to connect to WiFi...");
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to WiFi after 15 seconds");
    useUDP = false;
    return false;
  }
  else
  {
    Serial.printf("WiFi connected to %s; IP address: %s\n", bestSSID.c_str(), WiFi.localIP().toString().c_str());
    find_quisk_IP();
    udpCommand.begin(COMMAND_UDPPORT); // Initialize UDP for command port
    udpData.begin(DATA_UDPPORT);       // Initialize UDP for data port
    useUDP = true;
    data_sending = true;
    return true;
  }
}

void setup()
{
  mutex_init(&my_mutex);
  if (mutex_try_enter(&my_mutex, &mutex_save)) // Synchronize cores so they start about the same time.
  {
    if (useUDP)
    {
      SetupWiFi();  // We could setup UDP here and use the return value of SetupWiFi.
    }
    Serial.begin();
    pinMode(SMPS_ENABLE, OUTPUT);
    si5351_init();
    pinMode(RX_SWITCH, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT); // Set the LED pin as output.  It is used for TX mode.
    cur_mode = MODE_FT8;
    setup_mode(cur_mode);
    rx(); // Set RX mode
    mutex_exit(&my_mutex);
  }
  Serial.println("Setup done.");
}

void setup1()
{
  // We don't need core1 if using Audio and UART. \addindex.
  if (useUDP)
  {                 // This runs on Core1.  It is the I2S setup.
    i2s.setDATA(2); // These are the pins for the data on the SDR-TRX
    i2s.setBCLK(0);
    i2s.setMCLK(3);
    // Note: LRCK pin is BCK pin plus 1 (1 in this case).
    i2s.setSysClk(RATE);
    i2s.setBitsPerSample(24);
    i2s.setFrequency(RATE);
    i2s.setMCLKmult(MCLK_MULT);
    i2s.setBuffers(32, 0, 0);
    i2s.begin();
    mutex_enter_blocking(&my_mutex); // This should syschornize the cores, so one doesn't fill the buffer
    // before the other is ready.
    mutex_exit(&my_mutex);
  }
}

void loop()
{
  // Serial.printf("In loop, useUDP is %d\n", useUDP);
  if (useUDP)
  {
    processCommandUDP();
  }
  else
  {
    processCommandUART();
    // Serial.println("processCommandUART done.");
  }
  if (data_sending)
  {
    // This should run on Core0.  It is the UDP loop.
    static BufferEmptyer emptyer(bufferQueue);
    emptyer.emptyBuffer(); // Empty the buffer
  }
}

void loop1()
{ // This should run on Core1.  It is the I2S loop.
  if (useUDP)
  {
    // Serial.printf("In loop1.\n");
    if (data_sending)
    {
      static BufferFiller filler(bufferQueue);
      filler.fillBuffer(); // Fill the buffer
    }
  }
}