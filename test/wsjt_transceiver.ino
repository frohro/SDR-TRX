//WSJT Transceiver
//Agustin Martino
//LU2HES

// This will work, but only as a transmitter.  We will have the problem that
// two programs cannot talk to the same serial port at the same time. Quisk
// needs to control the serial port to talk to the radio.  So we will need to
// incorporate this into Quisk.  If we don't do this, we will not be able to use
// it as a transceiver over USB.

#include <si5351.h>
#include <rs_common.h>
#include <int.h>
#include <JTEncode.h>
#include <string.h>
#include "Wire.h"

// Mode defines
#define JT9_TONE_SPACING        174          // ~1.74 Hz
#define JT65_TONE_SPACING       269          // ~2.69 Hz
#define JT4_TONE_SPACING        437          // ~4.37 Hz
#define WSPR_TONE_SPACING       146          // ~1.46 Hz
#define FSQ_TONE_SPACING        879          // ~8.79 Hz
#define FT8_TONE_SPACING        625          // ~6.25 Hz
#define FT4_TONE_SPACING       2083          // ~20.83 Hz

#define JT9_DELAY               576          // Delay value for JT9-1
#define JT65_DELAY              371          // Delay in ms for JT65A
#define JT4_DELAY               229          // Delay value for JT4A 
#define WSPR_DELAY              682          // Delay value for WSPR
#define FSQ_2_DELAY             500          // Delay value for 2 baud FSQ
#define FSQ_3_DELAY             333          // Delay value for 3 baud FSQ
#define FSQ_4_5_DELAY           222          // Delay value for 4.5 baud FSQ
#define FSQ_6_DELAY             167          // Delay value for 6 baud FSQ
#define FT8_DELAY               157          // Delay value for FT8
#define FT4_DELAY                45          // Need to substract 3ms due to PLL delay

#define FT4_SYMBOL_COUNT 103

#define JT9_DEFAULT_FREQ        14078700UL
#define JT65_DEFAULT_FREQ       14078300UL
#define JT4_DEFAULT_FREQ        14078500UL
#define WSPR_DEFAULT_FREQ       14038600UL
#define FSQ_DEFAULT_FREQ        14005350UL     // Base freq is 1350 Hz higher than dial freq in USB
#define FT8_DEFAULT_FREQ        14074000UL
#define FT4_DEFAULT_FREQ        14047500UL

#define DEFAULT_MODE            MODE_FT8

// Hardware defines
#define BIAS  16
#define LED   15
#define RELAY 14
#define ATTENUATOR 2

#define SERIAL_TIMEOUT 20000

// Enumerations
enum mode {MODE_JT9, MODE_JT65, MODE_JT4, MODE_WSPR, MODE_FSQ_2, MODE_FSQ_3,
           MODE_FSQ_4_5, MODE_FSQ_6, MODE_FT8, MODE_FT4};

// Class instantiation
Si5351 si5351;

// Global variables
unsigned long freq;
uint16_t offset = 1200; //Baseband offset in Hz
uint8_t tx_buffer[255];
enum mode cur_mode = DEFAULT_MODE;
uint8_t symbol_count;
uint16_t tone_delay, tone_spacing;
bool message_available = false;
unsigned int timeout;


//Prepare transceiver for transmission (before receiving new symbols)
void pre_transmit()
{
  //T/R swith to TX position
  digitalWrite(RELAY, HIGH);
  digitalWrite(ATTENUATOR, HIGH);  
}

// Loop through the string, transmitting one character at a time.
void transmit()
{
  uint8_t i;
  unsigned long tx_freq;  
  tx_freq = freq + offset;

  // Reset the tone to the base frequency and turn on the output
  si5351.output_enable(SI5351_CLK2, 1);
  digitalWrite(BIAS, LOW);
  digitalWrite(LED, HIGH);

  // Now transmit the channel symbols
  if(cur_mode == MODE_FSQ_2 || cur_mode == MODE_FSQ_3 || cur_mode == MODE_FSQ_4_5 || cur_mode == MODE_FSQ_6)
  {
    uint8_t j = 0;
    while(tx_buffer[j++] != 0xff);
    symbol_count = j - 1;
  }

  for(i = 0; i < symbol_count; i++)
  {
    si5351.set_freq((tx_freq * 100ULL) + (tx_buffer[i] * tone_spacing), SI5351_CLK2);
    delay(tone_delay);
  }

  // Turn off the output
  digitalWrite(BIAS, HIGH);    
  si5351.output_enable(SI5351_CLK2, 0);
  digitalWrite(LED, LOW);

  // Back to receive
  delay(200);  
  digitalWrite(RELAY, LOW);
  digitalWrite(ATTENUATOR, LOW);  
}

void setup_mode(enum mode sel_mode)
{
  // Set the proper frequency, tone spacing, symbol count, and
  // tone delay depending on mode
  switch(sel_mode)
  {
  case MODE_JT9:
    freq = JT9_DEFAULT_FREQ;
    symbol_count = JT9_SYMBOL_COUNT; // From the library defines
    tone_spacing = JT9_TONE_SPACING;
    tone_delay = JT9_DELAY;
    break;
  case MODE_JT65:
    freq = JT65_DEFAULT_FREQ;
    symbol_count = JT65_SYMBOL_COUNT; // From the library defines
    tone_spacing = JT65_TONE_SPACING;
    tone_delay = JT65_DELAY;
    break;
  case MODE_JT4:
    freq = JT4_DEFAULT_FREQ;
    symbol_count = JT4_SYMBOL_COUNT; 
    tone_spacing = JT4_TONE_SPACING;
    tone_delay = JT4_DELAY;
    break;
  case MODE_WSPR:
    freq = WSPR_DEFAULT_FREQ;
    symbol_count = WSPR_SYMBOL_COUNT; // From the library defines
    tone_spacing = WSPR_TONE_SPACING;
    tone_delay = WSPR_DELAY;
    break;
  case MODE_FT8:
    freq = FT8_DEFAULT_FREQ;
    symbol_count = FT8_SYMBOL_COUNT; // From the library defines
    tone_spacing = FT8_TONE_SPACING;
    tone_delay = FT8_DELAY;
    break;
  case MODE_FT4:
    freq = FT4_DEFAULT_FREQ;
    symbol_count = FT4_SYMBOL_COUNT; 
    tone_spacing = FT4_TONE_SPACING;
    tone_delay = FT4_DELAY;
    break;    
  case MODE_FSQ_2:
    freq = FSQ_DEFAULT_FREQ;
    tone_spacing = FSQ_TONE_SPACING;
    tone_delay = FSQ_2_DELAY;
    break;
  case MODE_FSQ_3:
    freq = FSQ_DEFAULT_FREQ;
    tone_spacing = FSQ_TONE_SPACING;
    tone_delay = FSQ_3_DELAY;
    break;
  case MODE_FSQ_4_5:
    freq = FSQ_DEFAULT_FREQ;
    tone_spacing = FSQ_TONE_SPACING;
    tone_delay = FSQ_4_5_DELAY;
    break;
  case MODE_FSQ_6:
    freq = FSQ_DEFAULT_FREQ;
    tone_spacing = FSQ_TONE_SPACING;
    tone_delay = FSQ_6_DELAY;
    break;
  }
}

void setup()
{
  // Start serial and initialize the Si5351
  bool i2c_found;
  Serial.begin(57600);
  //Serial.println("Start setup");
  i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);

  // Configure Relay, LED and BIAS
  pinMode(BIAS, OUTPUT);    
  pinMode(RELAY, OUTPUT);
  pinMode(ATTENUATOR, OUTPUT);    
  pinMode(LED, OUTPUT);  
  digitalWrite(LED, LOW);
  digitalWrite(RELAY, LOW);
  digitalWrite(ATTENUATOR, LOW);  
  digitalWrite(BIAS, HIGH);  

  // Set the mode to use
  cur_mode = MODE_FT8;
  setup_mode(cur_mode);

  // Set CLK0 output
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA); // Set for max power
  si5351.output_enable(SI5351_CLK2, 0); // Disable the clock initially
  //Serial.println("End setup");
}


void loop()
{
  char recibido;
  unsigned char rec_byte[2];
  unsigned char msg_index;

  if (Serial.available() > 0)
  {
    // Read the incoming byte:
    recibido = Serial.read();
    
    // New message
    if (recibido == 'm')
    {
      msg_index = 0;
      timeout = 0;
      while (msg_index < symbol_count && timeout < SERIAL_TIMEOUT)
      {
        if(Serial.available() > 0)
        {
          recibido = Serial.read();
          tx_buffer[msg_index] = recibido;
          msg_index++;
        }
        timeout += 1;
      }
      if(timeout >= SERIAL_TIMEOUT)
      {
        message_available = false;        
        Serial.println("Timeout");
        Serial.println(timeout, DEC);        
        Serial.println(msg_index, DEC);
        // for(unsigned char kk=0; kk<symbol_count; kk++)
        //   Serial.println(tx_buffer[kk], DEC);
      }
      else
      {
        message_available = true;
        //Serial.println(timeout, DEC);
        //Serial.println(msg_index, DEC);              
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
        if(Serial.available() > 0)
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
      if(cur_mode == MODE_FT8)
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
      if(message_available)
        transmit();
    }

    //Pre transmit
    else if (recibido == 'p')
    {
      pre_transmit();
    }

    else if (recibido == 'r')
    {
      // Send indication for ready!
      Serial.print("r");    
    }
    
  }
  delay(10);  
}
