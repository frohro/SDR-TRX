from __future__ import print_function
from __future__ import absolute_import
from __future__ import division
import socket
import time
import yaml
import struct
import datetime
import sys
import os
from ft8 import FT8Send
from ft4 import FT4Send
from quisk_hardware_model import Hardware as BaseHardware

# SDR-TRX Quisk Configuration File
# This file is to integrate the function of control of the SDR-TRX wint Quisk and with WSJT-X.
# This is for control of the SDR-TRX with using the UDP port port 12346.  WSJT-X interfaces with this
# locally using UDP on port 2237.  Capital letters begin commands to the SDR-TRX from Quisk, 
# and lowercase letters are for commands coming from WSJT-X (through this Quisk interface).
# This is for usinng the SDR-TRX with a 3.5 mm audio card to the soundcard of the computer,
# and the WSJT-X software using UDP.
# It requires the libldl/ directory from weakmon to be in the directory you run this from.
# It also requires the WSJTXClass.py file from wsjtx_transceiver_interface to be in the lib directory
# and the version of that class. I have supplied a pruned down versions of ft8.py and ft4.45, because we don't need much
# other than FT8Send and FT4Send from those files.  You need to edit transceiver_config.yml to have your callsign and grid, etc.

import lib.WSJTXClass as WSJTXClass

# Import weakmon to use encoders

sys.path.append(os.path.expandvars('$WEAKMON'))

# SOUND CARD SETTINGS
#
# Uncomment these if you wish to use PortAudio directly
# name_of_sound_capt = "portaudio:(hw:2,0)"
# name_of_sound_play = "portaudio:(hw:1,0)"

# Uncomment these lines if you wish to use Pulseaudio
name_of_sound_capt = "pulse"
name_of_sound_play = "pulse"

# Radio's Frequency limits.
radio_lower = 3500000
radio_upper = 30000000

# Set the number of Hz the signal is tuned to above the center frequency to avoid 1/f noise.
vfo_Center_Offset = 10000



# SDR-TRX Hardware Control Class
#

class Hardware(BaseHardware):
    # These are "static variable substitutes" since python doesn't have them.
    # They are shared by all instances of the class and don't get redefinede each time a method is called.

    tx_ready_wsjtx = False
    tx_ready_wsjtx_sent = False
    tx_now = False

    # This is the open code for the WSJT server.
    ft8_encoder = FT8Send()
    ft4_encoder = FT4Send()

    def open(self):

        # Connection for WSJT-X
        self.PICO_UDP_IP = "192.168.1.107"  # Put the Pico IP here.
        self.COMMAND_UDP_PORT = 12346  # This is the port the Pico listens on for UDP comands coming from quisk.
        self.DATA_UDP_PORT = 12345  # This is the port the quisk listens on for UDP IQ data coming from the Pico W.
        self.command_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.command_sock.setblocking(False)
        self.data_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.data_sock.bind((self.PICO_UDP_IP, self.DATA_UDP_PORT))
        self.data_sock.setblocking(False)
        time.sleep(2)
        # Poll for version. Should probably confirm the response on this.
        version = str(self.get_parameter("VER"))  # The way the firmware is now, this sets it up to use the UART or UDP.
        # UART is used if get_parameter uses the UART, and UDP is used if it uses the UDP.
        print(version)
        # Return an informative message for the config screen
        t = str(version) + ". Capture from sound card %s." % self.conf.name_of_sound_capt
        configs_file = open('transceiver_config.yml', 'r')
        configs = yaml.load(configs_file, Loader=yaml.BaseLoader)

        # Global variables
        self.callsign = configs['callsign']
        self.grid = configs['grid']
        self.current_msg = ''
        self.rx_callsign = ''
        self.mode = 'FT8'
        self.tx_freq = 1200

        # Connection for WSJT-X
        WSJTX_UDP_IP = "127.0.0.1"  # Assuming you are running WSJT-X on the same computer as Quisk...
        WSJTX_UDP_PORT = 2237
        self.wsjtx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.wsjtx_sock.bind((WSJTX_UDP_IP, WSJTX_UDP_PORT))
        self.wsjtx_sock.setblocking(False)
        return t

    def close(self):
        # Called once to close the Hardware
        self.get_parameter("RX") # Make sure we are in RX mode.
        time.sleep(1)  # Wait for RX to be set.
        self.command_sock.close()
        self.wsjtx_sock.close()

    def ChangeFrequency(self, tune, vfo, source='', band='', event=None):
        # Called whenever quisk requests a frequency change.
        # This sends the FREQ command  dto set the centre frequency of the radio,
        # and will also move the 'tune' frequency (the section within the RX passband
        # which is to be demodulated) if it falls outside the passband (+/- sample_rate/2).
        print("Setting VFO to %d, with tune to %d." % (vfo, tune))
        print("Setting VFO to %d." % vfo)
        if vfo < radio_lower:
            vfo = radio_lower
            print("Outside range! Setting to %d" % radio_lower)

        if vfo > radio_upper:
            vfo = radio_upper
            print("Outside range! Setting to %d" % radio_upper)

        # print("sample_rate =", sample_rate)  # I believe sample_rate comes from quisk.
        # If the tune frequency is outside the RX bandwidth, set it to somewhere within that bandwidth.
        if tune > (vfo + sample_rate / 2) or tune < (vfo - sample_rate / 2):
            tune = vfo + vfo_Center_Offset
            print("Bringing tune frequency back into the RX bandwidth.")

        # success = self.set_parameter("FREQ",str(vfo))
        self.set_parameter("FREQ", str(vfo))
        self.set_parameter("TX_FREQ", str(tune))

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
    # UDP comms functions, to communicate with the Raspberry Pi Pico W board used in the SDR-TRX.
    #

    def get_parameter(self, string):
        string = string + "\n"
        self.command_sock.sendto(string.encode(), (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
        return self.get_argument()

    def set_parameter(self, string, arg):
        string = string + "," + arg + "\r" + "\n"
        self.command_sock.sendto(string.encode(), (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
        print('arg is: ', arg)
        temp_arg = self.get_argument()
        print('temp_arg is: ', temp_arg)
        return True

    def get_argument(self):
        self.command_sock.setblocking(True)
        try:
            data1 = self.command_sock.recv(1024)
            print('Received on line 178: ', data1)
        except Exception as e:
            print("Exception occurred: {0}".format(e))
            return -1
        self.command_sock.setblocking(False)
        if not data1.startswith(b'OK'):
            print('Received on line 185: ', data1)
        # Maybe we didn't catch an OK line?
        else:
            while True:
                try:
                    data1 = self.command_sock.recv(1024)
                    print('Received on line 190: ', data1)
                    break
                except BlockingIOError:
                    pass
        # Check to see if we have a comma in the string. If not, there is no argument.
        if data1.find(b',') == -1:
            return -1

        data1 = data1.split(b',')[1].rstrip(b'\r\n')
        print("data1 =")
        print(data1)

        # Check for the OK string.  Should we wait for the OK string?
        try:
            data2 = self.command_sock.recv(1024)
            if data2.startswith(b'OK'):  
                return data2
        except BlockingIOError:
            return -1

    # We might want to integrate the UDP reads below into the stuff above.

    #  The code below is to interface WSJT-X with SDR-TRX.

    def encode_ft8(self, msg):
        try:
            a77 = self.ft8_encoder.pack(msg, 1)
            symbols = self.ft8_encoder.make_symbols(a77)
        except Exception as e:
            print("FT8 encoder error, check message!")
            print(f"Exception type: {type(e).__name__}")
            print(f"Exception message: {str(e)}")
            symbols = None
            time.sleep(3)
        return symbols

    def encode_ft4(self, msg):
        try:
            a77 = self.ft4_encoder.pack(msg, 1)
            symbols = self.ft4_encoder.make_symbols(a77)
        except:
            print("FT4 encoder error, check message!")
            symbols = None
            time.sleep(3)
        return symbols

    def load_symbols(self, symbols):
        print("Load symbols into transmitter..")
        self.command_sock.sendto(b'm', (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
        count = 0
        for symbol in symbols:
            self.command_sock.sendto(struct.pack('>B', symbol), (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
            count += 1
            # Wait to avoid Arduino serial buffer overflow.  This may not be needed with the Pico.
            if count % 50 == 0:
                time.sleep(0.05)
        self.command_sock.sendto(b'\0', (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
        resp = self.command_sock.recv(1)
        if resp == b'm':
            print("Load OK")
        else:
            print(resp)

    def change_freq(self, new_freq):
        #global self.tx_freq
        self.command_sock.setblocking(True)
        print("Change TX frequency to:", new_freq)
        self.command_sock.sendto(b'o', (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
        self.command_sock.sendto(struct.pack('<H', new_freq), (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
        time.sleep(0.05)    
        resp = self.command_sock.recv(1)
        print("resp after changing offset is: ", resp)
        self.command_sock.setblocking(False)
        if resp == b'o':
            print("New freq OK")
            self.tx_freq = new_freq

    def set_mode(self, new_mode):  # New more robust protocol.
        #global mode
        if new_mode == 'FT8':
            self.command_sock.sendto(b'e', (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
            time.sleep(.05)
            resp = self.command_sock.read(1) 
            print("resp = ", resp)       
            if resp == b'e':
                self.mode = new_mode
                print("Switched to: {0}".format(new_mode)) 
                self.current_msg = '' 
                return True
        elif new_mode == 'FT4':
            self.command_sock.sendto(b'f', (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
            time.sleep(0.05)
            resp = self.command_sock.recv(1)
            print("resp = ", resp)       
            if resp == b'f':
                self.mode = new_mode
                self.current_msg = ''
                print("Switched to: {0}".format(new_mode))
                return True
        else:
            return False


    def new_msg(self, msg):
        #global self.current_msg
        #global self.mode
        if msg != self.current_msg:
            print("Message: {0}".format(msg))
            if 'FT8' in self.mode:
                symbols = self.encode_ft8(msg)
            else:
                symbols = self.encode_ft4(msg)
            if symbols.any():
                # symbols = [kk for kk in range(79)]
                self.load_symbols(symbols)
                self.current_msg = msg
            else:
                return
        else:
            time.sleep(0.005)  #  Do we need this?

    def transmit(self):
        if False:  # not current_msg:
            print("No previous message!")
            time.sleep(1)
        else:
            print("TX!")
            self.command_sock.sendto(b't', (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
            # self.tx_now = False

    def check_time_window(self, utc_time):
        time_window = 15 if 'FT8' in self.mode else 7
        print("Time window: ", time_window)  # Is correct for FT8 and FT4.
        rm = utc_time.second % time_window
        if rm > 1 and rm < time_window - 1:  # Within one second of exact time for FT8.
            return False
        else:
            return True

    # See if HeartBeat will work for this:
    def HeartBeat(self):  # Should be called at 10 Hz from the main.
        # Check transmitter is initialized
        #raise Exception("In HeartBeat")
    
        # print("\n\nWait for transmitter ready...")
        if self.tx_ready_wsjtx == False:
            if self.tx_ready_wsjtx_sent == False:
                self.command_sock.sendto(b'r', (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
                time.sleep(0.05)
                self.tx_ready_wsjtx_sent = True
            while True:
                try:
                    x = self.command_sock.recv(1)
                    if x == b'r':
                        print("Transmitter ready!")
                        self.tx_ready_wsjtx = True
                    break
                except BlockingIOError:
                    pass          
        if self.tx_ready_wsjtx == True:
            try:
                fileContent = self.wsjtx_sock.recv(1024)
            except Exception as e:
                if e == BlockingIOError:  # This is our way of pollig the socket.
                    # Maybe use select to stop the errors printing after we quit.
                    return BaseHardware.HeartBeat(self)
                else:
                    # print("Error in HeartBeat")
                    # print(f"Exception type: {type(e).__name__}")
                    # print(f"Exception message: {str(e)}")
                    pass
            try:
                NewPacket = WSJTXClass.WSJTX_Packet(fileContent, 0)
                NewPacket.Decode()

                if NewPacket.PacketType == 1:
                    # print('New Packet is type 1')
                    StatusPacket = WSJTXClass.WSJTX_Status(fileContent, NewPacket.index)
                    StatusPacket.Decode()
    
                    # Check TX frequency and update transceiver
                    new_freq = StatusPacket.TxDF
                    new_mode = StatusPacket.TxMode.strip()
                    

                    if new_freq != self.tx_freq:
                        # time.sleep(0.03)
                        print('Changing frequency from {0} to {1}'.format(self.tx_freq, new_freq))
                        try:
                            self.change_freq(new_freq)
                        except Exception as e:
                            print(f"Exception occurred: {e}")
                        print ('It is now ', self.tx_freq)

                    print('new_mode', new_mode, ' self.mode is: ', self.mode)
                    if new_mode != self.mode:  
                        print("Mode before: {0}".format(self.mode))
                        self.set_mode(new_mode)
                        print("New mode after: {0}".format(self.mode))

                    # Check if TX is enabled
                    if StatusPacket.Transmitting == 1:
                        # Check time, avoid transmitting out of the time slot
                        current_time = datetime.datetime.now()
                        utc_time = current_time.astimezone(datetime.timezone.utc)
                        self.tx_now = self.check_time_window(utc_time)
                        if self.tx_now:
                            self.command_sock.sendto(b'p', (self.PICO_UDP_IP, self.COMMAND_UDP_PORT))
                        message = StatusPacket.TxMessage
                        message = message.replace('<', '')
                        message = message.replace('>', '')
                        print('Message is: ', message)
                        self.new_msg(message.strip())
                        print("Encoded message is: ", self.current_msg)
                        # print("tx_now is: ", self.tx_now)
                        if self.tx_now:
                            print('Transmitting')
                            self.transmit()
                        print("Time: {0}:{1}:{2}".format(utc_time.hour, utc_time.minute, utc_time.second))
                    else:
                        print('Not Transmitting')
                    return BaseHardware.HeartBeat(self)
        
            except Exception as e:
                # print("Error in HeartBeat")
                # print(f"Exception type: {type(e).__name__}")
                # print(f"Exception message: {str(e)}")
                # traceback.print_exc()
                return BaseHardware.HeartBeat(self)
