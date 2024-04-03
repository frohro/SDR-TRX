# SDR-TRX Quisk Configuration File
# This file is to integrate the function of control of the SDR-TRX wint Quisk and with WSJT-X.
# This is for control of the SDR-TRX with using the UART interface.  WSJT-X interfaces with this
# locally using UDP.  Capital letters begin commands to the SDR-TRX from Quisk, 
# and lowercase letters are for commands coming from WSJT-X (through this Quisk interface).
# This is for usinng the SDR-TRX with a 3.5 mm audio card to the soundcard of the computer,
# and the WSJT-X software using the USB UART.

from __future__ import print_function
from __future__ import absolute_import
from __future__ import division

import socket
import time
import lib.WSJTXClass as WSJTXClass
import serial
import yaml
import struct
import datetime
import os

#Import weakmon to use encoders
import sys
import sys
import os

sys.path.append(os.path.expandvars('$WEAKMON'))
sys.path.append(os.path.join(os.path.dirname(__file__), '/home/frohro/Documents/PlatformIO/Projects/wsjtx_transceiver_interface'))
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from ft8 import FT8Send
from ft4 import FT4Send


# SOUND CARD SETTINGS
#
# Uncomment these if you wish to use PortAudio directly
#name_of_sound_capt = "portaudio:(hw:2,0)"
#name_of_sound_play = "portaudio:(hw:1,0)"

# Uncomment these lines if you wish to use Pulseaudio
name_of_sound_capt = "pulse"
name_of_sound_play = "pulse"

# Radio,s Frequency limits.
radio_lower = 3800000
radio_upper = 30000000

# Set the number of Hz the signal is tuned to above the center frequency.
vfo_Center_Offset = 10000
# SDR-TRX Hardware Control Class
#
import serial,time
from quisk_hardware_model import Hardware as BaseHardware

class Hardware(BaseHardware):
  def open(self):
    # SERIAL PORT SETTINGS

    # This is the open code for the WSJT server.
    # serial_port = configs['serial_port']
    # baudrate    = configs['baudrate']
    # try:
    #     self.or_serial = serial.Serial(serial_port, baudrate, timeout=0.5)
    # except serial.serialutil.SerialException:
    #     print("\nNo se puede abrir self.or_serial: " + serial_port + "\n")
    #     exit(1)
    # Set this as appropriate for your OS.
    radio_serial_port = "/home/frohro/ttyDummy"  # First port to try.
    radio_serial_rate = 57600  # Not nneeded for ACM serial port.  Could try commenting it out.
    # Called once to open the Hardware
    # Open the serial port.
    try:
        self.or_serial = serial.Serial(radio_serial_port,radio_serial_rate,timeout=.05)
    except serial.serialutil.SerialException:
        radio_serial_port = "/dev/ttyACM0" # Set to the port for interceptty.
        # To run interceptty: $ interceptty /dev/ttyACM0 /dev/ttyDummy
    try:
        self.or_serial = serial.Serial(radio_serial_port,radio_serial_rate,timeout=0.5)
    except serial.serialutil.SerialException:
        radio_serial_port = "/dev/ttyACM1" # Set to the third serial port for your OS.
    try:
        self.or_serial = serial.Serial(radio_serial_port,radio_serial_rate,timeout=0.5)
    except serial.serialutil.SerialException:
        print("Radio not connected to serial port.  Exiting.")
        exit(1)
        # Maybe use exit(1) instead.
    print("Opened Serial Port ",radio_serial_port)
    # Wait for the Pico to restart and boot.
        #Read configuration file
    configs_file = open('transceiver_config.yml', 'r')
    configs = yaml.load(configs_file, Loader=yaml.BaseLoader)

    #Global variables
    callsign = configs['callsign']
    grid = configs['grid']
    current_msg = ''
    rx_callsign = ''
    mode = 'FT8'
    tx_freq = 1200

    #Connection for WSJT-X
    UDP_IP = "127.0.0.1"
    UDP_PORT = 2237
    self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    self.sock.bind((UDP_IP, UDP_PORT))

    #FT8 encoder
    self.ft8_encoder = FT8Send()
    self.ft4_encoder = FT4Send()

    time.sleep(2)
    # Poll for version. Should probably confirm the response on this.
    version = str(self.get_parameter("VER"))  # The way the firmware is now, this sets it up to use the UART.
    print(version)
    # Return an informative message for the config screen
    t = str(version) + ". Capture from sound card %s." % self.conf.name_of_sound_capt
    return t

  def close(self):      
    # Called once to close the Hardware
    self.or_serial.close()
    self.sock.close()

  def ChangeFrequency(self, tune, vfo, source='', band='', event=None):
    # Called whenever quisk requests a frequency change.
    # This sends the FREQ command to set the centre frequency of the radio,
    # and will also move the 'tune' frequency (the section within the RX passband
    # which is to be demodulated) if it falls outside the passband (+/- sample_rate/2).
    print("Setting VFO to %d." % vfo)
    if(vfo<radio_lower):
      vfo = radio_lower
      print("Outside range! Setting to %d" % radio_lower)

    if(vfo>radio_upper):
      vfo = radio_upper
      print("Outside range! Setting to %d" % radio_upper)

    # success = self.set_parameter("FREQ",str(vfo))
    self.set_parameter("FREQ",str(vfo))

    print("sample_rate =", sample_rate)
    # If the tune frequency is outside the RX bandwidth, set it to somewhere within that bandwidth.
    if(tune>(vfo + sample_rate/2) or tune<(vfo - sample_rate/2)):
      tune = vfo + vfo_Center_Offset
      print("Bringing tune frequency back into the RX bandwidth.")

    # if success:
    #   print("Frequency change succeeded!")
    # else:
    #   print("Frequency change failed.")

    return tune, vfo
  
  def OnButtonPTT(self, event):
    btn = event.GetEventObject()
    if btn.GetValue():
        self.get_parameter("TX")
        print("TX")
    else:
        self.get_parameter("RX")
        print("RX")
    return BaseHardware.OnButtonPTT(self, event)

#
# Serial comms functions, to communicate with the Pi Pico board
#

  def get_parameter(self,string):
    string = string + "\n"
    self.or_serial.write(string.encode())
    return self.get_argument()
    #return string.encode()
        
  def set_parameter(self,string,arg):
    string = string+","+arg+"\r"+"\n"
    self.or_serial.write(string.encode())
    print('arg is: ', arg)
    temp_arg = self.get_argument()
    print('temp_arg is: ', temp_arg)
    return True
    # if temp_arg == arg:
    #   return True
    # else:
    #   return False
    
  def get_argument(self):
    data1 = self.or_serial.readline()
    # Do a couple of quick checks to see if there is useful data here
    if len(data1) == 0:
        return -1
    if not data1.startswith(b'OK'):
        print('Received: ', data1)
        
    # Maybe we didn't catch an OK line?
    else:
        data1 = self.or_serial.readline()
        print('Received: ', data1)
        
    # Check to see if we have a comma in the string. If not, there is no argument.
    if data1.find(b',') == -1:
        return -1
    
    data1 = data1.split(b',')[1].rstrip(b'\r\n')
    print("data1 =")
    print(data1)
    
    # Check for the OK string
    data2 = self.or_serial.readline()
    if data2.startswith(b'OK'):
        return data1

    # We need to integrate the UART reads below into the stuff above.


    #  The code below is to interface WSJT-X with SDR-TRX.



    def encode_ft8(msg):
        try:
            a77 = ft8_encoder.pack(msg, 1)
            symbols = ft8_encoder.make_symbols(a77)
        except:
            print("FT8 encoder error, check message!")
            symbols = None
            time.sleep(3)
        return symbols

    def encode_ft4(msg):
        try:
            a77 = self.ft4_encoder.pack(msg, 1)
            symbols = self.ft4_encoder.make_symbols(a77)
        except:
            print("FT4 encoder error, check message!")
            symbols = None
            time.sleep(3)
        return symbols

    def load_symbols(symbols):
        print("Load symbols into transmitter..")
        self.or_serial.write(b'm')
        count = 0
        for symbol in symbols:
            self.or_serial.write(struct.pack('>B', symbol))
            count += 1
            #Wait to avoid Arduino serial buffer overflow
            if count % 50 == 0:
                time.sleep(0.05)
        self.or_serial.write(b'\0')
        resp = self.or_serial.read(512)
        if resp == b'm':
            print("Load OK")
        else:
            print(resp)
        

    def change_freq(new_freq):
        global tx_freq
        print ("Change TX frequency to:", new_freq)
        self.or_serial.write(b'o')
        for kk in range(2):
            self.or_serial.write(struct.pack('>B', (new_freq >> 8*kk) & 0xFF))
        resp = self.or_serial.read(1)        
        if resp == b'o':
            print("New freq OK")
            tx_freq = new_freq
            

    def change_mode(new_mode):
        global mode
        self.or_serial.write(b's')
        resp = self.or_serial.read(1)        
        if resp == b's':
            mode = new_mode
            print("Switched to: {0}".format(new_mode))
            current_msg = ''    

    def new_msg(msg):
        global current_msg
        global mode
        if msg != current_msg:
            print("Message: {0}".format(msg))
            if 'FT8' in mode:
                symbols = encode_ft8(msg)
            else:
                symbols = encode_ft4(msg)            
            if symbols.any():
                #symbols = [kk for kk in range(79)]
                load_symbols(symbols)
                current_msg = msg
            else:
                return
        else:
            time.sleep(0.005)

    def transmit():
        if False:#not current_msg:
            print("No previous message!")
            time.sleep(1)
        else:
            print("TX!")
            self.or_serial.write(b't')                

    def check_time_window(utc_time):
        time_window = 15 if 'FT8' in mode else 7
        rm = utc_time.second % time_window
        if rm > 1 and rm < time_window-1:
            return False
        else:
            return True

    #Check transmitter is initialized
    print("\n\nWait for transmitter ready...")
    while True:
        time.sleep(1)    
        self.or_serial.write(b'r')
        x = self.or_serial.read()
        if x == b'r':
            print("Transmitter ready!")
            break

    # See if Heartbeat will work for this:
    def HeartBeat(self):  # Should be called at 10 Hz from the main.     
        try:
            fileContent, addr = self.sock.recvfrom(1024)
            NewPacket = WSJTXClass.WSJTX_Packet(fileContent, 0)
            NewPacket.Decode()

            if NewPacket.PacketType == 1:
                StatusPacket = WSJTXClass.WSJTX_Status(fileContent, NewPacket.index)
                StatusPacket.Decode()

                #Check TX frequency and update transceiver
                new_freq = StatusPacket.TxDF
                new_mode = StatusPacket.TxMode.strip()
                
                if new_freq != tx_freq:
                    change_freq(new_freq)

                if new_mode != mode:
                    change_mode(new_mode)

                #Check if TX is enabled
                if StatusPacket.Transmitting == 1:
                    #Check time, avoid transmitting out of the time slot
                    # utc_time = datetime.datetime.utcnow()
                    current_time = datetime.datetime.now()
                    utc_time = current_time.astimezone(datetime.timezone.utc)
                    tx_now = check_time_window(utc_time)                
                    if tx_now:
                        self.or_serial.write(b'p')
                    message = StatusPacket.TxMessage
                    message = message.replace('<', '')
                    message = message.replace('>', '')                
                    new_msg(message.strip())
                    if tx_now:
                        transmit()
                    print("Time: {0}:{1}:{2}".format(utc_time.hour, utc_time.minute, utc_time.second))
                    return BaseHardware.HeartBeat(self)
        except:
            print("Error in HeartBeat")
            return BaseHardware.HeartBeat(self)
