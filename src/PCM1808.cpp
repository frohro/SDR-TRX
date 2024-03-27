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

// To do:
// 1) We need to add I2C code eand calibrate thu INA219.
// 2) Refactor this into a library so you can do a receiver, transmitter
// or transceiver.  Use OOP.  Make the code maintainable.
// 3) We need to add a way to send the data in binary.
// 4) We need to add the transmitter control code.

// These are things you might want to change for your situation.
const int RATE = 16000;    // Your network needs to handle this.
const int MCLK_MULT = 256; //
const char *SSID = "Frohne-2.4GHz";
// const char *SSID = "rosbots";
// const char *PASSWORD = "ros2bots"; // In case you have a password,
// you must add this to the WiFi.begin() call, or if you have no
// passward, you need to take the PASSWORD arguement out.
const size_t BUFFER_SIZE = 1404; // Assuming the MTU is 1500 bytes...
char buffer[BUFFER_SIZE];
const unsigned int DATA_UDPPORT = 12345;
const unsigned int COMMAND_UDPPORT = 12346;
const char *VERSION_NUMBER = "0.1.0";
IPAddress remoteIp(192, 168, 1, 101); // This is the IP address of the computer running Quisk.
// It will be replaced with the IP of the competer that sends the command.
// Radio specific parameters go here.
const int FREQ_LIMIT_LOWER = 3500000;
const int FREQ_LIMIT_UPPER = 30000000;
const int RX_SWITCH = 10;          // GPIO pin for RX/TX switch (High is RX)
const int CAL_FACTOR = 1.00007587; // Calibration factor for the Si5351
uint_fast32_t rx_freq = 14074000;  // 20 meter FT8 frequency
uint_fast32_t tx_freq = 14074000;  // 20 meter FT8 frequency
int rx_relay_state = 0;            // 0 is RX, 1 is TX
int tx_state = 0;                  // 0 is off, 1 is on
bool data_sending = false;         // Are we sending data over UDP?
bool useUDP;                       // If false, use UART instead of UDP.
const int SERIAL_TIMEOUT = 20000;  // 20 seconds
WiFiUDP udpData;                   // UDP object for data packets  (Is this a server or a client?)
WiFiUDP udpCommand;                // UDP object for control packets

Si5351 si5351(0x60); // 0x60 is the normal I2C address for the Si5351A

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

const int FT4_SYMBOL_COUNT = 103;

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
  freq = (uint64_t)(freq * 100ULL * CAL_FACTOR);
  pll_freq = (uint64_t)mult * freq;
  si5351.set_freq_manual(freq * CAL_FACTOR, pll_freq, SI5351_CLK0);
  si5351.set_freq_manual(freq * CAL_FACTOR, pll_freq, SI5351_CLK1);
  // Now we can set CLK1 to have a 90 deg phase shift by entering
  // mult in the CLK1 phase register, since the ratio of the PLL to
  // the clock frequency is mult.
  si5351.set_phase(SI5351_CLK0, 0);
  si5351.set_phase(SI5351_CLK1, mult);
  // We need to reset the PLL before they will be in phase alignment
  si5351.pll_reset(SI5351_PLLA);
}

void tx_enable()
{ // This can be used for CW keying.
  si5351.output_enable(SI5351_CLK2, 1);
  tx_state = 1;
}

void tx_disable()
{
  si5351.output_enable(SI5351_CLK2, 0);
  tx_state = 0;
}

void tx()
{
  if (useUDP)
  {
    data_sending = false;
  }
  digitalWrite(RX_SWITCH, LOW);    // Set TX mode on T/R switch
  digitalWrite(LED_BUILTIN, HIGH); // Turn on the LED
  delay(1000);
  rx_relay_state = 0;
  tx_enable();
}

void rx()
{ // Do we really need two functions for rx and tx? maybe a parameter?
  if (useUDP)
  {
    data_sending = true;
  }
  tx_disable();
  digitalWrite(RX_SWITCH, HIGH);
  digitalWrite(LED_BUILTIN, LOW); // Turn off the LED
  rx_relay_state = 1;
}

void set_tx_freq(uint32_t freq)
{
  // Let driver choose PLL settings. May glitch when changing frequencies.
  freq = (uint64_t)(freq * 100ULL * CAL_FACTOR);
  si5351.set_freq(freq, SI5351_CLK2);
  tx_freq = freq;
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
    si5351.set_freq((digital_tx_freq) + (tx_buffer[i] * tone_spacing), SI5351_CLK2);
    // si5351.set_freq((14100000*100ULL), SI5351_CLK2);
    // Serial.printf("digital_tx_freq: %d \r\n", digital_tx_freq);
    delay(tone_delay);
  }
  // Back to receive
  delay(200);
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
    tx_freq = FT8_DEFAULT_FREQ;
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
  set_tx_freq(tx_freq);
  delay(1000); // Wait for the PLL to lock (probably not needed
  set_rx_freq(tx_freq);
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
    remoteIp = udpCommand.remoteIP();
    if (command.startsWith("VER"))
    {
      udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
      Serial.println("VER received!");
      Serial.println("Began Packet.");
      Serial.printf("VER,%s\r\n", VERSION_NUMBER);
      udpCommand.printf("VER,%s\r\n", VERSION_NUMBER);
      udpCommand.endPacket();
      // useUDP = true; // Set useUDP to true
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
        udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
        udpCommand.printf("FREQ,%d\r\n", freq);
        udpCommand.endPacket();
      }
      else
      {
        udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
        udpCommand.printf("FREQ,%d\r\n", rx_freq);
        udpCommand.endPacket();
      }
    }
    else if (command.startsWith("TX_DISABLE"))
    {
      Serial.println("TX_DISABLE received!");
      tx_disable();
      udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
      udpCommand.printf("TX_DISABLE\r\n");
      udpCommand.endPacket();
    }
    else if (command.startsWith("TX_ENABLE"))
    {
      Serial.println("TX_ENABLE received!");
      tx_enable();
      udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
      udpCommand.printf("TX_ENABLE\r\n");
      udpCommand.endPacket();
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
        udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
        udpCommand.printf("TX_FREQ,%d\r\n", freq);
        udpCommand.endPacket();
      }
      else
      {
        udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
        udpCommand.printf("TX_FREQ,%d\r\n", tx_freq);
        udpCommand.endPacket();
      }
    }
    else if (command.startsWith("RX_FREQ"))
    {
      Serial.println("RX_FREQ received!");
      int commaIndex = command.indexOf(',');
      if (commaIndex != -1)
      {
        String freqStr = command.substring(commaIndex + 1);
        uint32_t freq = freqStr.toInt();
        set_rx_freq(freq);
        udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
        udpCommand.printf("RX_FREQ,%d\r\n", freq);
        udpCommand.endPacket();
      }
      else
      {
        udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
        udpCommand.printf("RX_FREQ,%d\r\n", rx_freq);
        udpCommand.endPacket();
      }
    }
    else if (command.startsWith("USE_UART"))
    {
      useUDP = false; // Set useUDP to false
      udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
      udpCommand.printf("USE_UART\r\n");
      udpCommand.endPacket();
    }
    else if (command.startsWith("TX"))
    {
      Serial.println("TX received!");
      tx();
      udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
      udpCommand.printf("TX\r\n");
      udpCommand.endPacket();
    }
    else if (command.startsWith("RX"))
    {
      Serial.println("RX received!");
      rx();
      udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
      udpCommand.printf("RX\r\n");
      udpCommand.endPacket();
    }
    else
    {

      Serial.println("Unknown command received!");
      udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
      udpCommand.write("ERROR\r\n");
      udpCommand.endPacket();
      no_error = false;
    }
    if (no_error)
    {
      udpCommand.beginPacket(remoteIp, udpCommand.remotePort());
      udpCommand.write("OK\r\n"); // Do we need an Ok after an error?
      udpCommand.endPacket();
    }
  }
}

void sendDataUDP()
{
  static int32_t r, l;
  size_t bufferIndex = 0;
  // Fill the buffer with 108 lines of "%d\r\n"
  while (bufferIndex < BUFFER_SIZE - 26)
  {                     // Change to 23?  23 is the max size of "%d,%d\r\n" with 10 digits
    i2s.read32(&l, &r); // Read the next l and r values
    l = l << 9;
    r = r << 9;
    // This next line is for transferring data over the UART in case you don't have a Pico W,
    // Serial.printf("%d,%d\r\n", l, r); // This only works up 16 kHz.
    // With the extra four spaces it should wark at 96 kHz with 16 bit samples
    // and leaving about 90 frames for control packets if we can figure out how
    // to do that.
    // These next lines create the buffer of slightly less than the MTU (1500 bytes)
    int n = snprintf(buffer + bufferIndex, BUFFER_SIZE - bufferIndex, "%d,%d\r\n", l, r);
    if (n > 0)
    {
      bufferIndex += n;
    }
    else
    {
      Serial.println("Problem with buffer!");
      break;
    }
  }
  // Send the data buffer via UDP
  // udpData.beginPacket(udpCommand.remoteIP(), DATA_UDPPORT); // Same IP for both ports.
  udpData.beginPacket(remoteIp, DATA_UDPPORT);
  udpData.write((const uint8_t *)buffer, bufferIndex);
  udpData.endPacket();
  // Clear the buffer for the next round
  memset(buffer, 0, BUFFER_SIZE);
}

void processCommandUART()
{
  bool no_error;
  String command;
  char recibido;
  unsigned char rec_byte[2];
  unsigned char msg_index;
  // Check if there's any serial data available
  if (Serial.available() > 0)
    recibido = Serial.read();
    // Serial.print(recibido);
  if ((recibido == 'm') || // This whole if statement is a way of combining my code with 
    (recibido == 'o') || // wsjt-transceiver.ino.  I'm not sure if it is the best way.
    (recibido == 's') || 
    (recibido == 'w') || 
    (recibido == 't') || 
    (recibido == 'p') || 
    (recibido == 'r') || 
    (recibido < 32)) // Check for a message which is a control character in ASCII.
  {
    // New message
    if (recibido == 'm')
    {
      msg_index = 0;
      timeout = 0;
      while (msg_index < symbol_count && timeout < SERIAL_TIMEOUT)
      {
        if (Serial.available() > 0)
        {
          recibido = Serial.read();
          tx_buffer[msg_index] = recibido;
          msg_index++;
        }
        timeout += 1;
      }
      if (timeout >= SERIAL_TIMEOUT)
      {
        message_available = false;
        Serial.println("Timeout");
        Serial.println(timeout, DEC);
        Serial.println(msg_index, DEC);
      }
      else
      {
        message_available = true;
        Serial.print("m");
      }
    }

    // Change offset
    else if (recibido == 'o')
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

    // Switch mode
    else if (recibido == 's')
    {
      if (cur_mode == MODE_FT8)
        cur_mode = MODE_FT4;
      else
        cur_mode = MODE_FT8;
      setup_mode(cur_mode);
      message_available = false;
      Serial.print("s");
    }

    // WSPR Mode
    else if (recibido == 'w')
    {
      cur_mode = MODE_WSPR;
      setup_mode(cur_mode);
      message_available = false;
    }

    // Transmit
    else if (recibido == 't')
    {
      // digitalWrite(LED_BUILTIN, HIGH);
      // delay(1000);
      if (message_available)
        transmit();
    }

    // Pre transmit
    else if (recibido == 'p')
    {
      tx_enable();
    }
    else if (recibido == 'r')
    {
      // Send indication for ready!
      Serial.print("r");
    }
  }
  else
  {
    command = recibido + Serial.readStringUntil('\n');
    no_error = true;
    // Process the received command
    if (command.startsWith("VER"))
    {
      Serial.printf("VER,%s\r\n", VERSION_NUMBER);
      // useUDP = false; // Set useUDP to false
      //  The first thing Quisk does is to check VER.
      //  So if it uses UART, we will use UART.
    }
    else if (command.startsWith("START_I2S_UDP"))
    {
      Serial.println("START_I2S_UDP received!");
      data_sending = true;
      i2s.begin();
    }
    else if (command.startsWith("STOP_I2S_UDP"))
    {
      Serial.println("STOP_I2S_UDP received!");
      data_sending = false;
      i2s.end();
    }
    else if (command.startsWith("FREQ"))
    {
      int commaIndex = command.indexOf(',');
      if (commaIndex != -1)
      {
        String freqStr = command.substring(commaIndex + 1);
        uint32_t freq = freqStr.toInt();
        set_rx_freq(freq);
        set_tx_freq(freq);
        Serial.printf("FREQ,%d\r\n", freq);
      }
      else
      {
        Serial.printf("FREQ,%d\r\n", rx_freq);
      }
    }
    else if (command.startsWith("TX_DISABLE"))
    {
      tx_disable();
      Serial.printf("TX_DISABLE\r\n");
    }
    else if (command.startsWith("TX_ENABLE"))
    {
      tx_enable();
      Serial.printf("TX_ENABLE\r\n");
    }
    else if (command.startsWith("TX_FREQ"))
    {
      int commaIndex = command.indexOf(',');
      if (commaIndex != -1)
      {
        String freqStr = command.substring(commaIndex + 1);
        uint32_t freq = freqStr.toInt();
        set_tx_freq(freq);
        Serial.printf("TX_FREQ,%d\r\n", freq);
      }
      else
      {
        Serial.printf("TX_FREQ,%d\r\n", tx_freq);
      }
    }
    else if (command.startsWith("RX_FREQ"))
    {
      int commaIndex = command.indexOf(',');
      if (commaIndex != -1)
      {
        String freqStr = command.substring(commaIndex + 1);
        uint32_t freq = freqStr.toInt();
        set_rx_freq(freq);
        Serial.printf("RX_FREQ,%d\r\n", freq);
      }
      else
      {
        Serial.printf("RX_FREQ,%d\r\n", rx_freq);
      }
    }

    else if (command.startsWith("USE_UDP"))
    {
      useUDP = true; // Set useUDP to true
      Serial.printf("USE_UDP\r\n");
    }
    else if (command.startsWith("TX"))
    {
      tx();
      tx_enable();
      Serial.printf("TX\r\n");
    }
    else if (command.startsWith("RX"))
    {
      rx();
      tx_disable();
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

void setup()
{
  Serial.begin(); // Pico uses /dev/ttyACM0.  No baud rate needed.
  i2s.setDATA(2); // These are the pins for the data on the SDR-TRX
  i2s.setBCLK(0);
  i2s.setMCLK(3);
  // Note: LRCK pin is BCK pin plus 1 (1 in this case).
  i2s.setBitsPerSample(24);
  i2s.setFrequency(RATE);
  i2s.setMCLKmult(MCLK_MULT);
  i2s.setSysClk(RATE);
  i2s.setBuffers(32, 0, 0);
  // WiFi.begin(SSID, PASSWORD); // Add , PASSWORD after SSID if you have one.
  WiFi.begin(SSID); // Add , PASSWORD after SSID if you have one.
  for (int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; i++)
  {
    delay(1000); // Delay for 1 second
    Serial.println("Trying to connect to WiFi...");
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to WiFi after 15 seconds");
    useUDP = false;
  }
  else
  {
    // Serial.println("Connected to WiFi");
    // Serial.println("WiFi connected");
    // Serial.print("Pico IP address: ");
    // Serial.println(WiFi.localIP());
    udpCommand.begin(COMMAND_UDPPORT); // Initialize UDP for command port
    udpData.begin(DATA_UDPPORT);       // Initialize UDP for data port
    useUDP = true;
  }
  si5351_init();
  pinMode(RX_SWITCH, OUTPUT);
  digitalWrite(RX_SWITCH, HIGH); // Set RX mode
  pinMode(LED_BUILTIN, OUTPUT);  // Set the LED pin as output.  It is used for TX mode.
  i2s.begin();
  cur_mode = MODE_FT8;
  setup_mode(cur_mode);
  set_tx_freq(FT8_DEFAULT_FREQ);
  delay(1000);
  set_rx_freq(FT8_DEFAULT_FREQ);
  rx();
  useUDP = false; // Only for testing out FT8
}

void loop()
{
  if (useUDP)
  {
    processCommandUDP();
  }
  else
  {
    processCommandUART();
  }
  // if (data_sending)
  //  if (udpData.remoteIP() != IPAddress(0, 0, 0, 0))
  {
    sendDataUDP();
  }
}
